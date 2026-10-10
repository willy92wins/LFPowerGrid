# INFORME — i76 G-01 ronda 3 (spec + implementacion)

## Hecho

Reparto multifuente: water-fill sobre la demanda LastStable (hard+soft),
oferta `m_OfferedResidual` (centinela -1), dirty B1.3, remaining M1.
Politica de overload all-off **sin tocar**.

## Slice vs bucle (criterio 3)

Opcion **a** para el cuerpo: `OfferCapFromWritten`, `ComputeOfferTowardEdge`,
`WaterFillShareAsk`, `WaterFillLeftoverAdd`, `EdgeHardPortion`,
`SkipOtherIndex`, `ShouldNotifyOfferDirty`, `ComputeOfferBase*`.
Opcion **b** para los `for` de `ApplyMergerWaterFill` / cola: el test
repite el bucle llamando esos cuerpos. No hay oraculo Python del split
comparado consigo mismo.

## Tests

`C:\Python314\python.exe .github/tools/test_graph_multifeed_split.py`
→ 23 OK (incluye 2.a–g, B1–B3, M1/M2, negativos, 463464e sin helpers).

`enforce_checks.py --root .` FAIL=0. Resto de `test_*.py` OK
(capacity, charger, reference, finish_wiring, enforce_checks).

Base `463464e`: `WaterFillShareAsk` ausente;
`edgeDemand = edgeDemand / ptPoweredIn` presente. El test
`test_base_commit_lacks_helpers` lo comprueba. Equal-split 0+25+25
falla `Oracle.verify` (`hard_unmet`, `partial_allocation`,
`feasible_but_underfed`).

## Linter

`script_validator.py .` → `errors: []` (status WARN, exit 2). Delta de
errores 0. Warnings preexistentes (ES-EMPTY-IFDEF, etc.).

## I4

`ApplyMergerWaterFill` sale al instante si no hay target con
`CountPoweredIncoming>1`. Pass 2/3 y `overloaded = totalHardDemand >
availableOutput + eps` son el texto de la base. Test
`test_i4_single_provider_matches_divisor_absent`: SOURCE 50 → load 10
asigna 10, no overload. B1.3 no sucia consumidores.

## HALLAZGOS-ADYACENTES

- Ratio hard/soft en Pass 1–3: si Σcap está entre hard y D, el surplus
  de Pass 3 compite con otras salidas soft (ya existía).
- `CountPoweredIncoming` sigue usando `m_OutputPower` del SOURCE.
- All-off del Combiner en déficit (2.b) tira inflow; fuera de G-01.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce / arranque del mundo.
- Linter en la base 463464e corrido en este worktree (solo `errors: []`
  del arbol actual).
- Cadenas SpA→SpB→C in-game (limitacion B1.3).
- `ca < cb` en Enforce para `MergerSortLess` (no hay compilador).

## PENDIENTE-INGAME (§6.3 revisado)

Montaje 2.a: `LFPG_SolarPanel` (20) + `LFPG_SolarPanel_T2` (50) →
`LFPG_Combiner` → `LFPG_WaterPump` (50). De día, ambos paneles ON,
bomba ON, ningún panel CRITICAL.

2.b: + nevera 20 + lámpara 10 (hard 80). Paneles sin overload;
Combiner CRITICAL; bomba apagada.

2.c: T2 → Splitter → (lámpara 10 + Combiner); panel 20 → Combiner;
bomba 50. l1 y bomba ON.

2.g: T2 a bomba 60 **y** Combiner; C all-off; bomba del Combiner
apagada.

Recuperación: cortar la bomba del Combiner; cables IDLE en ≤ 3 ticks
(`LFPG_PROPAGATE_TICK_MS`). Isla ajena intacta.

Lecturas: SyncVars `m_Overloaded` / `m_LoadRatio` del panel y Combiner.
