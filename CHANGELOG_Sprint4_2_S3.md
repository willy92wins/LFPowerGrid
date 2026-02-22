# CHANGELOG — Sprint 4.2 Session 3 (Cleanup / Dead Code Removal)
**Version**: v0.7.21
**Date**: 2026-02-22
**Base**: v0.7.20 post-S2b

## Summary

S3 removes all legacy BFS propagation code that became dead after Sprint 4.2 established the graph-based event-driven propagation system. The graph is now the **sole propagation path** — no legacy fallback remains.

---

## Dead Code Removed (LFPG_NetworkManager.c)

### Functions Removed

| Function | Lines | Reason |
|----------|-------|--------|
| `PropagateFromLegacy()` | ~150 | Never called. Graph propagation replaced it entirely. |
| `AllocateDeviceLoad()` | ~100 | Only called from PropagateFromLegacy (recursive load alloc). |
| `TurnOffDownstream()` | ~58 | Only called from PropagateFromLegacy and AllocateDeviceLoad. |
| `PurgeStaleReachable()` | ~37 | Operated on m_LastReachableBySource which was never populated. |
| `GraphRebuild()` | ~8 | Dead wrapper. ValidateAllWiresAndPropagate calls m_Graph.RebuildFromWires() directly. PostBulkRebuildAndPropagate calls m_Graph.PostBulkRebuild(). |

### Fields Removed

| Field | Reason |
|-------|--------|
| `m_LastReachableBySource` | Only populated by PropagateFromLegacy (never called). |
| `m_PropagateQueued` | Only used inside PropagateFromLegacy (never called). |

### Constructor Cleanup

Removed initialization of `m_LastReachableBySource` and `m_PropagateQueued` from `LFPG_NetworkManager()`.

---

## Comment Updates

| File | Change |
|------|--------|
| **LFPG_NetworkManager.c** header | Removed "PropagateFrom() is legacy" reference. Removed "DEPRECATED for propagation" on ReverseIdx. Updated to v0.7.21. |
| **LFPG_ElecGraph.c** header | Added S3 note: "Dead code removal — graph is now sole propagation path." Updated to v0.7.21. |
| **LFPG_Splitter.c** `LFPG_SetPowered` | Replaced stale "BFS traverses through Splitter" comment with accurate graph-based propagation description (PASSTHROUGH + DIRTY_INPUT). |
| **LFPG_PlayerRPC.c** CutWires handler | Cleaned stale "replaces old buggy pattern" comment — just describes PostBulkRebuildAndPropagate sequence. |
| **LFPG_PlayerRPC.c** CutPort handler | Same cleanup as CutWires. |

---

## Version Bump

`LFPG_VERSION_STR` updated from `"0.7.20"` to `"0.7.21"` in LFPG_Defines.c.

---

## Line Delta

| File | Before (S2b) | After (S3) | Delta |
|------|-------------|------------|-------|
| LFPG_NetworkManager.c | 1989 | 1591 | **-398** |
| LFPG_ElecGraph.c | 1321 | 1324 | +3 |
| LFPG_Defines.c | 316 | 316 | 0 |
| LFPG_PlayerRPC.c | 893 | 884 | -9 |
| LFPG_Splitter.c | 535 | 536 | +1 |
| **Total codebase** | **~13,476** | **~13,073** | **-403** |

---

## Observations for Sprint 4.3

During the audit, two minor performance observations were noted (not bugs, appropriate for future optimization):

1. **ProcessDirtyQueue budget waste on dedup**: When a node is dequeued but skipped via the `m_LastEpoch == m_CurrentEpoch` check, the `processed` counter is still incremented, consuming budget without doing useful work. In pathological cases with many intra-epoch re-enqueues, this could reduce effective throughput. Consider moving the counter increment after the dedup check in Sprint 4.3.

2. **ResetRequeueCounts iterates all nodes**: Called at the start of every epoch, it iterates ALL nodes (O(N_total)) even when only a few are dirty. For large graphs with sparse dirty sets, a targeted reset (only nodes that were actually enqueued) would be more efficient. Could be addressed alongside EDGE_BUDGET implementation in Sprint 4.3.

---

## Sprint 4.2 Session Status (Updated)

| Session | Status | Scope |
|---------|--------|-------|
| S1 Infraestructura | ✅ | MarkNodeDirty, ProcessDirtyQueue, SyncNodeToEntity, TickPropagation, warmup |
| S2 Coherencia (Audit 1) | ✅ | H1 bool, H2 bulk order, H3 epoch requeue, H4 head-index queue |
| S2b Modelo (Audit 2) | ✅ | H2 dirty mask diff, H3 warmup budget, H6 consumption, H7 epoch docs |
| **S3 Limpieza** | **✅** | **Dead code removal (-403 lines), comment updates, version bump** |
| S4 Audit externo + cierre | ⬜ | Audit final, handoff a 4.3, context update |
