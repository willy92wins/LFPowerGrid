# Informe: inspector SOURCE overload

## Cambio
La fuente encendida usa `m_SnapshotBoolD` (`LFPG_DeviceAPI.GetOverloaded`) para `#STR_LFPG_INSPECT_OVERLOAD` en rojo, sin barra ni %. Con flag falso: `#STR_LFPG_INSPECT_ACTIVE` + `BuildLoadBar` + %. Carga justa (ratio 1.00) queda ACTIVO 100 %.

## Diff resumido
- `scripts/4_World/LFPG_DeviceInspector.c`: miembro `m_SnapshotBoolD`; `boolD` en `ClientDataChanged`; SOURCE asigna `GetOverloaded`; `changed` y persistencia del snapshot; render SOURCE por flag.
- `.github/tools/test_inspector_source_overload.py`: P1, P2, N1, M1 (bloques aa96d35 como literales).
- `.github/workflows/checks.yml`: paso al final.

## Gates
- dayz-script-validator: `errors: []` (len=0). `status` WARN por patrones `#if` preexistentes; el gate es `len(errors)==0`.
- `enforce_checks.py --root .`: FAIL=0 WARN=0 (269 text / 161 Enforce).
- `test_inspector_source_overload.py`: 4 tests OK (P1, P2, N1, M1).
- Resto de `.github/tools/test_*.py`: todos OK (broadcast, cutwires, enforce_checks, finish_wiring_quota, graph_capacity_refresh, graph_charger_energy, graph_multifeed_split, graph_perf_probe, graph_reference).
- Gate A EOL: inspector.c y checks.yml CRLF (sin LF suelto); test nuevo LF (157 líneas LF, 0 CRLF).
- Gate B: no aplica en esta lane (sin commit; lo corre el orquestador tras el commit).

## Fuera de alcance
- Telemetría en `scripts/5_Mission/LFPG_ElecGraphImpl.c:3175`: `loadState = "OVERLOADED"` si `node.m_LoadRatio >= LFPG_LOAD_CRITICAL_THRESHOLD`. Mismo umbral `>=` que el inspector viejo; no se toca (R20).
- `LFPG_LOAD_CRITICAL_THRESHOLD` permanece; único uso restante es esa telemetría.
- Cables en rojo / grafo all-off: ya viven en el grafo; el inspector solo refleja `m_Overloaded`.
