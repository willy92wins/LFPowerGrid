# NOTES — Luna exec L02

MODEL: grok-4.7

Write-set: `scripts/5_Mission/LFPG_ElecGraphImpl.c` only.
Base: `b0c94386e60044dadd24edf179839fcb29337331` (main still at that SHA when this branch was cut).

## Conteo heurístico

Regla por línea: vacía = blanco; si el trim empieza por `//` o `/*`, o la línea sigue dentro de un bloque `/* */`, = comentario; el resto = exec. `#if` / `#ifdef` / `#ifndef` / `#else` / `#endif` cuentan como exec.

| | total | exec | comentario | blanco | #if* | #endif |
|---|---:|---:|---:|---:|---:|---:|
| antes (b0c9438) | 4384 | 3105 | 831 | 448 | 34 | 34 |
| r0 (81e539c) | 4351 | 3072 | 830 | 449 | 34 | 34 |
| r1 (este tip) | 4356 | 3076 | 831 | 449 | 36 | 36 |

- Exec vs b0c9438: 3105 → 3076 (−29). El dedupe sigue en −33; r1 suma 4 exec (`#ifdef SERVER` + `#endif` × 2).
- Comentario vs b0c9438: 831 → 831 (0). r0 había dejado −1; r1 restaura el bloque H5 (4 líneas) en lugar del resumen de 3.
- Blanco vs b0c9438: 448 → 449 (+1 separador). No se vende como exec.

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

r0 no tocó directivas (34 = 34). r1 añade dos pares porque no había un `#ifdef SERVER` adyacente donde meter el helper sin mover otro código a través de una guarda. Los métodos vecinos abren su `#ifdef` dentro del cuerpo, no alrededor de la definición.

| | #if* | #endif | profundidad final |
|---|---:|---:|---:|
| b0c9438 | 34 | 34 | 0 |
| r1 | 36 | 36 | 0 |

Guardas nuevas, las dos a profundidad 0, sin `#else`:

- `DropRemovedNodeMaps`: `#ifdef SERVER` en la línea del método, `#endif` justo después de su cierre.
- `ReadLiveSourceOn`: igual, entre el separador "Internal helpers" y `EnsureNode`.

Equivalencia:

- Con `SERVER` definido: las dos definiciones existen. Las cinco llamadas siguen dentro de los `#ifdef SERVER` de `OnDeviceRemoved`, `CleanupOrphanNode`, `EnsureNode`, `PopulateAllNodeElecStates` y `RefreshSourceState`. Mismo orden de efectos que r0.
- Sin `SERVER`: definiciones y call sites desaparecen juntos. Ningún `#else` cambió. Los `return` de cliente de los métodos públicos no cambian.

## Fix r1 (rechazo Sol de #37)

- B1: definiciones de `DropRemovedNodeMaps` y `ReadLiveSourceOn` envueltas en `#ifdef SERVER` / `#endif`. El cuerpo del dedupe no se movió.
- B2: el resumen de tres líneas sobre los mapas vuelve al texto H5 de b0c9438 (disparador: el nodo principal no pasa por `CleanupOrphanNode`; coste: `m_NodeNetLow` / `m_NodeNetHigh` crecen sin límite con la rotación de dispositivos).

## Riesgo residual y no verificado

- Sin compilador Enforce y sin arranque in-game. `script_validator.py` no está en esta VM.
- `AGENTS.md` pide no commitear; el brief manda el commit en `chore/luna-exec-l02`. Manda el brief.
