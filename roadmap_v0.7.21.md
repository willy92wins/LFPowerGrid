# LF_PowerGrid — Contexto v0.7.21 (Sprint 4.2 S3)

## Estado actual

LF_PowerGrid v0.7.21 — Sprint 4.2 S1+S2+S2b+S3 completados. Motor de propagación event-driven sobre grafo como **único camino de propagación** (legacy BFS eliminado). Dirty queue budgeted, semántica diferenciada de dirty masks, modelo de consumo real para consumidores, y warmup con budget elevado. Pendiente S4 (audit final + handoff a 4.3).

## Auditorías integradas

**Auditoría 1** (pre-S2, sobre v0.7.19):
- H1 (ALTO): OnWireAdded no propagaba fallo → **Corregido S2**
- H2 (CRÍTICO): Orden incorrecto en CutWires/CutPort → **Corregido S2**
- H3 (MEDIO): Requeue per-epoch mal reseteado → **Corregido S2**
- H4 (MEDIO-BAJO): Cola dirty con copias de array por tick → **Corregido S2**

**Auditoría 2** (sobre v0.7.19, cruzada contra v0.7.20):
- H1 = Auditoría 1 H3 → **Ya corregido en S2**
- H2 (MEDIO): Dirty masks sin semántica diferenciada → **Corregido S2b**
- H3 (MEDIO-BAJO): EDGE/WARMUP budget no usados → **Corregido S2b** (WARMUP activo, EDGE reserved)
- H4 = roadmap desactualizado → **Ya corregido en S2**
- H5 = changelog incompleto → **Ya corregido en S2**
- H6 (MEDIO): Consumer ignora m_Consumption → **Corregido S2b**
- H7 (BAJO): Epoch naming ambiguo → **Documentado S2b**

**Estado: todos los hallazgos de ambas auditorías resueltos o documentados.**

## Sprint 4.2 — Estado por sesiones

| Sesión | Estado | Scope |
|--------|--------|-------|
| S1 Infraestructura | ✅ | MarkNodeDirty, ProcessDirtyQueue, SyncNodeToEntity, TickPropagation, warmup |
| S2 Coherencia (Audit 1) | ✅ | H1 bool, H2 bulk order, H3 epoch requeue, H4 head-index queue |
| S2b Modelo (Audit 2) | ✅ | H2 dirty mask diff, H3 warmup budget, H6 consumption, H7 epoch docs |
| S3 Limpieza | ✅ | Dead code removal (-403 lines), comment updates, version bump |
| S4 Audit externo + cierre | ⬜ | Audit final, handoff a 4.3, context update |

## S3 Dead Code Removal Summary

Removed **398 lines** from LFPG_NetworkManager.c — todo el árbol de propagación legacy BFS:
- `PropagateFromLegacy()` — nunca llamado tras Sprint 4.2
- `AllocateDeviceLoad()` — solo llamado desde PropagateFromLegacy
- `TurnOffDownstream()` — solo llamado desde PropagateFromLegacy/AllocateDeviceLoad
- `PurgeStaleReachable()` — operaba sobre m_LastReachableBySource (nunca poblado)
- `GraphRebuild()` — wrapper muerto, callers usan m_Graph directamente
- Campos: `m_LastReachableBySource`, `m_PropagateQueued`

Actualizados comentarios obsoletos en Splitter (BFS → graph), PlayerRPC (stale audit notes).

## Observaciones para Sprint 4.3

1. **Budget waste on dedup**: `processed` incrementa para nodos skipped por epoch check → menor throughput efectivo en caso patológico.
2. **ResetRequeueCounts O(N_total)**: Itera todos los nodos cada epoch; targeted reset sería más eficiente con grafos grandes y dirty sets dispersos.

### Pruebas funcionales pendientes

- [ ] B1. Warmup startup con save existente (20-50 nodos)
- [ ] B2. Incremental connect: FinishWiring + NotifyGraphWireAdded (bool)
- [ ] B3. Incremental disconnect: CutPort OUT → PostBulkRebuildAndPropagate
- [ ] B4. CutWires bulk → PostBulkRebuildAndPropagate
- [ ] B5. Ciclo: rechaza sin residuos en grafo/persistencia
- [ ] B6. Budget overflow: red > 64 nodos dirty, drain parcial + continuación
- [ ] B7. Self-heal: RequestGlobalSelfHeal → rebuild + warmup mode
- [ ] B8. Consumer con consumption declarado → off si input < m_Consumption
- [ ] B9. Consumer legacy (consumption=0) → on con cualquier input > epsilon
- [ ] B10. Source toggle → DIRTY_INTERNAL → skip input eval → propaga
- [ ] B11. Warmup budget: logs muestran budget=128 en startup, luego 64

## Codebase

| Archivo | Líneas | Rol |
|---------|--------|-----|
| LFPG_CableRenderer.c | 2362 | 3D cable rendering |
| LFPG_NetworkManager.c | 1591 | Server singleton (wires, propagation, persistence) |
| LFPG_ElecGraph.c | 1324 | Electrical graph (nodes, edges, dirty queue) |
| LFPG_Actions.c | 1127 | Player actions |
| LFPG_PlayerRPC.c | 884 | RPC validation + cycle check + graph hooks |
| LFPG_WiringClient.c | 825 | Client wiring state machine |
| LFPG_TestDevices.c | 759 | Test devices |
| LFPG_IDevice.c | 675 | Device API |
| LFPG_Splitter.c | 536 | Splitter |
| LFPG_Defines.c | 316 | Constants, enums, budgets, epoch docs |
| Total | ~13,073 | 30 archivos |

## Sprints

| Sprint | Versión | Estado |
|--------|---------|--------|
| 1 | v0.7.11 | ✅ Hardening |
| 2 | v0.7.12 | ✅ Coherencia C/S |
| 2.5 | v0.7.13-14 | ✅ QA / Telemetría |
| 3 | v0.7.15 | ✅ Persistencia |
| HF | v0.7.16 | ✅ Audit fixes |
| 4.1 | v0.7.17 | ✅ Topología grafo |
| **4.2** | **v0.7.21** | **🟨 S1→S3 done, S4 pending** |
| 4.3 | v0.7.22+ | ⬜ Modelo de carga/overload |
| 5 | TBD | ⬜ UI Foundation |
| 6 | TBD | ⬜ Component Expansion |

## Sprint 4.3 — Modelo de Carga (próximo)

Prereqs ya completados en 4.2:
- ✅ m_MaxOutput y m_Consumption poblados y usados
- ✅ m_Priority y m_Flags en ElecEdge
- ✅ Consumer respeta m_Consumption real
- ✅ EDGE_BUDGET reservado
- ✅ ProcessDirtyQueue calcula inputSum con split por edges
- ✅ Legacy BFS eliminado — grafo es único camino de propagación

Sprint 4.3 scope:
1. Overload detection: totalDemand > sourceCapacity → priorización por edge
2. Load allocation: m_Priority en ElecEdge determina orden de servicio
3. Brownout policy: off binario por ahora
4. Edge budget activo: limitar fan-out spikes
5. Visual: cable colors → WARNING_LOAD (80%), CRITICAL_LOAD (100%)
6. Telemetría: latencia de propagación, carga por componente
7. ProcessDirtyQueue optimizations: budget skip fix, targeted ResetRequeueCounts

## Principios de escalabilidad

1. Client-side: sistemas con budget, no escalar con contenido total del servidor
2. Server-side: lógica eléctrica event-driven, no polling
3. Propagación incremental: solo nodos afectados
4. Bulk mutations: rebuild completo + warmup budget

## Enforce Script rules

No ternarios, no ++/--, tipos explícitos, null checks, NaN guards, #ifdef SERVER, map.Find() para lookups.
