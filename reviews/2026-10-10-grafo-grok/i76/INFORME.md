# INFORME — i76 G-01 ronda 5 (B5-1 re-dirty incremental)

## Hecho

Un hermano sin corriente se recalcula cuando entra otro proveedor.
`NotifyOfferChanged` marca incoming enabled del target sin
`CountPoweredIncoming > 1`. Un PASSTHROUGH que cruza epsilon de
suministro notifica aunque la oferta no cambie
(`PassthroughSupplyAt` + `SupplierVisibilityFlipped` sobre
`m_PrevInputPower` / `m_InputPower`). Sin campos nuevos.

## Tests

`C:\Python314\python.exe .github/tools/test_graph_multifeed_split.py`
→ 38 OK (29 previos + C/F partida, propiedad last/cut/perm/toggle,
negativos de flags, estructurales).

- `test_enforce_checks.py` 21 OK
- `test_graph_capacity_refresh.py` 6 OK
- `test_graph_charger_energy.py` 12 OK
- `test_finish_wiring_quota.py` 7 OK
- `test_broadcast_contract.py` 3 OK
- `test_graph_reference.py` 20 OK
- `enforce_checks.py --root .` FAIL=0 WARN=0 exit 0
- `script_validator.py .` `len(errors)=0` status WARN exit 2

## B5-1

C con panel → C el último, y panel apagado/encendido: 20+30, lámpara
10, bomba 50, nadie en overload. F con panel último y con generador →
Sp último: bomba del Splitter 50, esp 0, e20 20, C overload, fuentes
sin overload.

## HALLAZGOS-ADYACENTES

- La oferta de un PT no depende de su entrada real (base = ofertas
  incoming). Puede entrar al vector de un hermano con oferta 0→0.
- Nodos sin suministro: el motor limpia overload (`:2510`); la
  propiedad no los cuenta en el conjunto.

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce / mundo in-game en esta lane.
- Linter contra 463464e en este worktree.
- Cadenas SpA→SpB→C in-game.

## PENDIENTE-INGAME (§6.3)

Montaje C: generador 50 → Splitter → (lámpara PT 10 + Combiner);
panel 20 al Combiner **el último**; Combiner → bomba PT 50. Tras el
cable y tras noche/día del panel: lámpara y bomba ON, paneles sin
CRITICAL.

Montaje F: bomba PT 50 en el Splitter. Panel último o generador →
Splitter último: bomba del Splitter ON, Combiner CRITICAL, fuentes
sin overload.
