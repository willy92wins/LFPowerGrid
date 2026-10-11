# INFORME — i76 G-01 ronda 4 (B4-1, M4-1, M4-2, SPEC)

## Hecho

Un PASSTHROUGH sin entrada sigue en el vector de merger (B4-1).
`MergerSortLess` compara enteros `ToAscii`. Ofertas O(salidas): hard
una vez + `ComputeOfferTowardEdge(baseP, totalHard, ownHard)`. Water-fill
devuelve bool; totales solo si reescribio. Visitas de edge al presupuesto
PDQ. SPEC §3 alineada con el codigo.

## Tests

`C:\Python314\python.exe .github/tools/test_graph_multifeed_split.py`
→ 29 OK (2.a–g, B1–B4-1, M1/M2, S3, 2.d cola, negativos).

Resto:
- `test_enforce_checks.py` 21 OK
- `test_graph_capacity_refresh.py` 6 OK
- `test_graph_charger_energy.py` 12 OK
- `test_finish_wiring_quota.py` 7 OK
- `test_broadcast_contract.py` 3 OK
- `test_graph_reference.py` 18 OK
- `enforce_checks.py --root .` FAIL=0 WARN=0 exit 0

Base `463464e`: `WaterFillShareAsk` ausente;
`edgeDemand = edgeDemand / ptPoweredIn` presente.

## Linter

`script_validator.py .` → `errors: []` (status WARN, exit 2). `len(errors)=0`.
Warnings preexistentes (ES-EMPTY-IFDEF, ES-GETTYPE). Delta de errores 0.

## B4-1

`IncludeInMergerVector(true, 0)` incluye el propio edge. Tras apagar y
encender S50: l1 ON, S50 sin overload, bomba ON (tres hermanos, un
hermano, edges paralelos). Sin la rama `fromSelfNode` el test 1 falla.

## M4-1

`MergerSortLess` recorre caracteres con `Substring` + `ToAscii` (mismo
patron que `LFPG_RPCServerHandlerImpl.c:2520-2521`). Compara `codeA <
codeB` y longitudes `int`. No hay `string < string` en el diff.

## M4-2

Borrados `OtherEnabledHard` y `SkipOtherIndex`. Recalc de totales
condicionado a `ApplyMergerWaterFill`. Visitas en Publish, MergerVector
y water-fill.

## I4

Sin vector de tamaño ≥ 2 no se reescribe. Pass 2/3 y overload son los
de la base.

## HALLAZGOS-ADYACENTES

- Cadenas SpA→SpB→C: B1.3 no sucia SpB (limitacion previa).
- All-off del Combiner en deficit (2.b) tira inflow; fuera de G-01.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce / arranque del mundo.
- Linter en la base 463464e corrido en este worktree (solo `errors: []`
  del arbol actual).
- Cadenas SpA→SpB→C in-game.

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

B4-1: T2 → Splitter (lámpara + Combiner); S20/S30 → Combiner; T2
también a Splitter vía Sp. Apagar/encender el T2 de Sp: lámpara ON,
T2 sin overload.

Recuperación: cortar la bomba del Combiner; cables IDLE en ≤ 3 ticks
(`LFPG_PROPAGATE_TICK_MS`). Isla ajena intacta.

Lecturas: SyncVars `m_Overloaded` / `m_LoadRatio` del panel y Combiner.
