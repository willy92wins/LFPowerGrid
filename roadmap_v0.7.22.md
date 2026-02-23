# LF_PowerGrid — Contexto v0.7.22 (Sprint 4.3 Complete)

## Estado actual

LF_PowerGrid v0.7.22 — Sprints 4.1 + 4.2 + 4.3 completados. Motor de propagación event-driven sobre grafo con modelo de carga completo: detección de overload, asignación por prioridad, política de brownout binario, edge budget activo, colores visuales WARNING/CRITICAL, y telemetría de propagación server-side. Legacy BFS eliminado desde 4.2 S3.

## Auditorías integradas

**Auditoría 1** (pre-S2, sobre v0.7.19):
- H1-H4: Todos corregidos en Sprint 4.2 S2.

**Auditoría 2** (sobre v0.7.19, cruzada contra v0.7.20):
- H1-H7: Todos corregidos o documentados en Sprint 4.2 S2b.

**Auditoría 3** (Sprint 4.3 self-audit, v0.7.22):
- H1: EdgeIndex stale tras RemoveFromOutgoing → **Corregido 4.3** (reindexing + origIdx guard)
- H2: LFPG_LOAD_TELEM_DELTA definida pero no usada → **Corregido 4.3** (SyncNodeToEntity per-source logging)
- H3: NetworkManager header v0.7.21 → **Corregido 4.3** (bump a v0.7.22)

**Estado: todos los hallazgos de todas las auditorías resueltos.**

## Sprint 4.2 — Completado

| Sesión | Estado | Scope |
|--------|--------|-------|
| S1 Infraestructura | ✅ | MarkNodeDirty, ProcessDirtyQueue, SyncNodeToEntity, TickPropagation, warmup |
| S2 Coherencia (Audit 1) | ✅ | H1 bool, H2 bulk order, H3 epoch requeue, H4 head-index queue |
| S2b Modelo (Audit 2) | ✅ | H2 dirty mask diff, H3 warmup budget, H6 consumption, H7 epoch docs |
| S3 Limpieza | ✅ | Dead code removal (-403 lines), comment updates, version bump |
| S4 Audit + cierre | ✅ | Merged into 4.3 (handoff was implicit, audit done in 4.3) |

## Sprint 4.3 — Completado

| Item | Estado | Scope |
|------|--------|-------|
| Overload detection | ✅ | totalDemand > sourceCapacity → AllocateOutputByPriority |
| Load allocation | ✅ | m_Priority sort → greedy allocation → m_AllocatedPower per edge |
| Brownout policy | ✅ | Binary: LFPG_EDGE_BROWNOUT flag, fully served or fully denied |
| Edge budget | ✅ | LFPG_PROPAGATE_EDGE_BUDGET=256 active, m_EdgesVisitedThisEpoch tracking |
| Visual feedback | ✅ | WARNING_LOAD (amber ≥80%), CRITICAL_LOAD (orange ≥100%) in CableRenderer |
| Telemetry | ✅ | Server-side periodic dump (latency, edges, overloads) + per-source load events |
| PDQ optimizations | ✅ | Budget dedup fix + targeted ResetRequeueCounts O(dirty) |
| EdgeIndex fix | ✅ | RemoveFromOutgoing reindex + AllocateOutputByPriority origIdx guard |

## Pruebas funcionales (Sprint 4.2+4.3)

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
- [ ] B12. Overload: 3 consumers en 1 source → prioridad ordena servicio, último en brownout
- [ ] B13. Brownout visual: cable brownout muestra CRITICAL_LOAD color (orange)
- [ ] B14. Warning visual: source al 85% load muestra WARNING_LOAD color (amber)
- [ ] B15. Edge budget: red con >256 edges per tick → drain parcial correcto
- [ ] B16. Telemetry dump: [Telemetry-Propagation] aparece cada ~5s en server RPT
- [ ] B17. Per-source log: [LoadTelem] aparece al conectar/desconectar consumers

## Codebase

| Archivo | Líneas | Rol |
|---------|--------|-----|
| LFPG_CableRenderer.c | 2362 | 3D cable rendering + WARNING/CRITICAL visual states |
| LFPG_NetworkManager.c | ~1650 | Server singleton (wires, propagation, persistence, load telemetry) |
| LFPG_ElecGraph.c | ~1570 | Electrical graph (nodes, edges, dirty queue, load allocation) |
| LFPG_Actions.c | 1127 | Player actions |
| LFPG_PlayerRPC.c | 884 | RPC validation + cycle check + graph hooks |
| LFPG_WiringClient.c | 825 | Client wiring state machine |
| LFPG_TestDevices.c | 759 | Test devices (LoadRatio, OverloadMask net sync) |
| LFPG_IDevice.c | 675 | Device API (SetLoadRatio, SetOverloadMask) |
| LFPG_Splitter.c | 536 | Splitter |
| LFPG_Defines.c | 297 | Constants, enums, budgets, edge flags, telemetry thresholds |
| Total | ~13,304 | 30 archivos |

## Sprints

| Sprint | Versión | Estado |
|--------|---------|--------|
| 1 | v0.7.11 | ✅ Hardening |
| 2 | v0.7.12 | ✅ Coherencia C/S |
| 2.5 | v0.7.13-14 | ✅ QA / Telemetría |
| 3 | v0.7.15 | ✅ Persistencia |
| HF | v0.7.16 | ✅ Audit fixes |
| 4.1 | v0.7.17 | ✅ Topología grafo |
| 4.2 | v0.7.21 | ✅ Propagación event-driven |
| **4.3** | **v0.7.22** | **✅ Modelo de carga** |
| 5 | TBD | ⬜ UI Foundation |
| 6 | TBD | ⬜ Component Expansion |

## Sprint 5 — UI Foundation (próximo)

Prereqs completados en 4.1-4.3:
- ✅ Grafo eléctrico completo con propagación event-driven
- ✅ Modelo de carga con overload/brownout
- ✅ Telemetría server-side y client-side
- ✅ Cable visual feedback (WARNING/CRITICAL colors)
- ✅ Net-synced LoadRatio y OverloadMask en devices

Sprint 5 scope (pendiente de definición detallada):
1. Contextual device info panel (load ratio, powered state, connections)
2. Wire inspection mode (click to see allocated power per edge)
3. Network overview (component count, total load summary)
4. Possible: Dabs Framework MVC evaluation for UI architecture

## Principios de escalabilidad

1. Client-side: sistemas con budget, no escalar con contenido total del servidor
2. Server-side: lógica eléctrica event-driven, no polling
3. Propagación incremental: solo nodos afectados
4. Bulk mutations: rebuild completo + warmup budget
5. Telemetry: periodic dumps, no per-frame logging (Sprint 4.3)

## Enforce Script rules

No ternarios, no ++/--, tipos explícitos, null checks, NaN guards, #ifdef SERVER, map.Find() para lookups.
