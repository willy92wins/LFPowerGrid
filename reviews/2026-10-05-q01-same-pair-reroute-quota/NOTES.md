# Q-01 — same-pair reroute blocked by the wire quota

Base: `origin/main` = `9f95c666b6ea5601d238cddd8630bc2b93782db0`.

## Bug

`HandleFinishWiring` called `CanPlayerCreateAnotherWire` and returned before
`FinishWiringSamePair` ran. A player at `MaxWiresPerPlayer` could not change the
route of their own wire, even though a same-pair reroute edits the row in place
and adds no wire (`FinishWiringReroute` only changes counts when `m_CreatorId`
changes).

## Fix (shape 1 + 2 together)

`scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`:

- The quota is still queried at the same spot, into `quotaFree`. The early denial
  is gone.
- Right after `FinishWiringSamePair`, before the reroute and before any graph or
  store mutation, `!quotaFree && FinishWiringNeedsQuotaSlot(rerouteWire, creatorId)`
  unlocks the port and denies with the same log and client message as before.
- `FinishWiringNeedsQuotaSlot`: a slot is needed for a new wire (`rerouteWire`
  null) and for a reroute that takes over another creator's row, because
  `FinishWiringReroute` then adds one to the sender. A reroute of the sender's own
  row needs no slot.

`CanPlayerCreateAnotherWire` is unchanged (this is its only caller).
`AllowCutOthersWires` is unchanged. Replacements on the normal path still need a
free slot, as SEC03 already requires.

Side effect of deferring: a quota-full player now reaches `ValidateWire`,
component-size, cycle, port-lock, ambiguity and collect checks before the
quota denial. All of them deny on their own. The visible change is that a
quota-full client sending an invalid route gets `ValidateWire`'s answer, and
with `KickOnInvalidWire` it is kicked. Before, it got "Wire limit reached".

## Tests

`.github/tools/test_finish_wiring_quota.py`, added to `checks.yml`. It runs
`FinishWiringSamePair`, `FinishWiringNeedsQuotaSlot` and the actual guard
condition from `HandleFinishWiring` as source slices (`enforce_scalar_slice.py`):

- quota full + same-pair reroute of own wire → allowed
- quota full + new wire / other target / two conflicts → blocked
- quota full + same-pair row of another creator → blocked
- free quota → every shape allowed
- the quota result is not acted on before the same-pair lookup, and the denial
  unlocks the port and comes before reroute, graph, store and count changes
- negative controls: the pre-fix early return, and a helper forced to `true`
  or `false`, each make the tests fail

```
python .github/tools/test_enforce_checks.py
python .github/tools/test_graph_capacity_refresh.py
python .github/tools/enforce_checks.py --root .
python .github/tools/test_graph_charger_energy.py
python .github/tools/test_finish_wiring_quota.py
python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .
```

In-game verification is still pending: Enforce compiles only when the world loads.
