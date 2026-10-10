# INFORME lane i78 — medicion R-01, R-02, R-04, R-05

| punto | veredicto | ficheros:lineas | prueba |
|---|---|---|---|
| R-01 CutAll | HECHO | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:982` Begin R01, fases index/rescue/rebuild, End `:1148` | `test_graph_perf_probe.py` test_r01 + negativo HEAD |
| R-02 MarkUpstreamNodesDirty | HECHO | `scripts/5_Mission/LFPG_ElecGraphImpl.c:3753` Begin R02, allocs `:3757`, End `:3807` | test_r02 + HEAD |
| R-04 CutPort IN + rescue | HECHO | `LFPG_RPCServerHandlerImpl.c:2084` Begin R04, fases, End `:2123`; alloc rescue `:2152` | test_r04 |
| R-05 GetElement/GetKey | HECHO | `LFPG_ElecGraphImpl.c` CountMapSweep en 9 barridos (`:391`, `:974`, `:1249`, `:1264`, `:1941`, `:2026`, `:2136`, `:2153`, `:3619`, `:3636`) | test_r05 (>=8) |
| Apagado por defecto | HECHO | `scripts/3_Game/LFPG_Defines.c` `LFPG_PERF_PROBE = false` | test_probe_is_off_by_default; negativo true |
| Acumulador | HECHO | `scripts/3_Game/LFPG_PerfProbe.c` | logs `LFPG_PERF` clave=valor |
| Settlement ticks | HECHO | `OnProcessDirtyQueue` en PDQ empty `:2235` y final `:2941` | PENDIENTE-INGAME |
| Test CI | HECHO | `.github/tools/test_graph_perf_probe.py`, paso en `.github/workflows/checks.yml` | 10 tests OK |
| MEDICION.md | HECHO | `reviews/2026-10-10-grafo-grok/i78/MEDICION.md` | inventario R-05 |

## R-01

Se anadio `LFPG_PerfProbe.Begin("R01")` tras resolver `deviceId` en `HandleCutWires` (`LFPG_RPCServerHandlerImpl.c:982`), fases `index`/`rescue`/`rebuild` alrededor del reverse-index, RescueStaleIncomingWires y PostBulkRebuildAndPropagate, y `End()` al salir. `AddAlloc(2)` en los `new` de deltas LFPG.

Opcion: constante `LFPG_PERF_PROBE` en Defines (un booleano por sitio). Se descarto `#define` en Defines.c porque Enforce no incluye cabeceras: el define no seria visible en 5_Mission.

Verificacion: offline test_r01. PENDIENTE-INGAME: protocolo R-01 en MEDICION.md.

## R-02

`MarkUpstreamNodesDirty` (`LFPG_ElecGraphImpl.c:3753`) cuenta las 2 allocations reales (array+map), requeues y visitas por arista, fase `walk`. No se unen raices.

Se reutiliza `m_LastProcessMs` / `m_EdgesVisitedThisEpoch` en `OnProcessDirtyQueue` para ticks de settlement.

Verificacion: test_r02. PENDIENTE-INGAME: cadena PT con consumos solapados.

## R-04

Begin solo en rama IN (`:2084`) para no medir cortes OUT. Rescue sigue ejecutandose siempre (cero cambio de comportamiento). End si `portDir == IN`.

Verificacion: test_r04. PENDIENTE-INGAME: indice sano vs parcial.

## R-05

`CountMapSweep` una vez por barrido (un booleano si el probe esta apagado), no por iteracion. Inventario completo en MEDICION.md; instrumentacion solo en ElecGraphImpl (lista blanca). CableRenderer cliente no se toco.

Contrato O(n): `P:\scripts\1_core\proto\enscript.c:861` y `:871`.

## Apagado y salida

`static const bool LFPG_PERF_PROBE = false` y `LFPG_PERF_PROBE_SUMMARY_MS = 5000`. Sin campos persistidos, sin settings, sin RPC. Logs `LFPG_Util.Info` formato `LFPG_PERF event=1 route=... clave=valor` mas `kind=summary` e `kind=sweep`.

## GATES

- Linter base (antes de editar): errors=0 warnings=48
- Linter final: errors=0 warnings=48 (delta errors 0; warnings igual)
- `python .github/tools/test_graph_perf_probe.py`: exit 0, 10 tests
- `test_graph_capacity_refresh.py`: exit 0, 6 tests
- `test_graph_charger_energy.py`: exit 0, 12 tests
- `test_finish_wiring_quota.py`: exit 0, 7 tests
- `test_enforce_checks.py`: exit 0, 21 tests
- `enforce_checks.py --root .`: FAIL=0 WARN=0
- Tests de arreglo contra HEAD: `test_negative_head_base_lacks_probes` hace `git show HEAD:...` y comprueba que R01/R02/R04/R05 no estan en la base (el test pasa ahora porque la base NO tiene probes; los positivos fallarian si se corrieran sobre el arbol de HEAD sin este diff)

## HALLAZGOS-ADYACENTES

- `HandleCutWires` / `HandleCutPort` tienen el caracter `â€"` en strings de log (encoding); no tocado.
- `LFPG_Telemetry.c` es `#ifndef SERVER`; no sirve para el grafo.
- NetworkManager y CableRenderer concentran mas GetElement/GetKey que el grafo; fuera de lista blanca.
- `AllocateOutput` y zona de cargadores no se tocaron (otras lanes).

## LO QUE NO PUDE VERIFICAR

- Compilacion Enforce al cargar el mundo (no hay compilador invocable).
- Coste real in-game con LFPG_PERF_PROBE true (PENDIENTE-INGAME).
- Que el bool false se elimine por el compilador (no hay evidencia de DCE en Enforce).
- Ticks de settlement reales con cola sucia.
- Completitud de Rescue vs indice en un mundo vivo.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

R-01 no es una violacion por si sola (el issue lo dice: T5-03). Medir PostBulkRebuild en CutAll puede mostrar un coste grande que es recuperacion correcta, no un bug. R-04 no puede omitir el scan por un hit: si la premisa de “el indice esta sano” no se puede probar con owners+contadores, el scan global es el comportamiento correcto y optimizarlo seria un riesgo de persistencia. R-05 mezcla hot path de cliente (CableRenderer, cada frame) con rebuilds de servidor infrecuentes; un solo umbral de refactor mezclaria ambos. El flag runtime (bool) no es coste cero absoluto: cada sitio paga un `if (LFPG_PERF_PROBE)` porque un `#define` en Defines.c no cruza ficheros.
