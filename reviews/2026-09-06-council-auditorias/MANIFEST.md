tipo: COMPARABLE

# MANIFIESTO — council-lfpowergrid-auditorias

- **run_id:** `2026-09-07-council-auditorias`
- **tipo:** `COMPARABLE` — brief byte-idéntico en las tres lanes, sin prefijo por lane, sin `--roles`.
- **pregunta:** Dadas las dos auditorías y el triage ya hecho, ¿cuál es el plan de acción conjunto y en qué orden? En concreto: ¿se jubila la V3 del sorter antes o después de arreglar los P1 de integridad?
- **decision_de_producto (justifica N=3, regla C2):** el orden entre *huella* (jubilar V3, objetivo declarado del dueño) e *integridad* (25 fichas P1 vivas, varias de pérdida de dinero y salto de autoridad). Las dos auditorías dan órdenes incompatibles —la grande pone la consolidación UI en la PR 11 de 12, el proyecto la está haciendo primera— y el conflicto **no se resuelve leyendo el árbol**: depende de qué valora el dueño y de si el fork es un impuesto mayor que la deuda de integridad. El orquestador ya hizo la parte que sí se resuelve leyendo (triage de las 25 P1 + censo de acoplamientos V3); lo que queda es juicio.
- **brief_path:** `BRIEF.md`
- **brief_sha256:** `4157d7e6c8a61b30e2934e5ad59014518694a99be68f92a6f5df0cf025f74cc1`
- **arbitro:** el orquestador (Claude/Anthropic). **No es lane**: ninguna de las tres lanes es Anthropic, así que no hay `arbitro=lane`.
- **fallback:** si una lane muere, la corrida sigue con dos y se declara N=2 en el arbitraje. No se sustituye modelo en silencio.

## Cifras del brief y su universo (regla C8)

| Cifra | Universo |
|---|---|
| 151 fichas | tabla del §4 del informe grande; `grep -c '^| \['` = 151 |
| 25 P1 | misma tabla, columna prioridad = P1 (20 Confirmado + 5 Potencial) |
| 24 VIVO + 1 PARCIAL | 6 por pre-filtro de fichero intacto + 19 por lectura dirigida; `TRIAGE-P1.md` |
| 34 fichas en ficheros intactos | intersección de ficheros citados por ficha contra `git diff --name-only --ignore-all-space d61705e..HEAD` (82 ficheros con cambio real) |
| V3 = 143.010 B / 7 clases; V4 = 178.279 B / 10 clases | `stat -c%s` y `grep -cE '^\s*(modded\s+)?class\s+\w+'` sobre los ficheros de cada UI |
| 286 clases, 2.818.739 B | mismo grep/`cat|wc -c` recursivo sobre `scripts/` |
| 106 referencias a V3 fuera de la V3 | `grep -cE 'LFPG_SorterView\.|LFPG_ColorData'` por fichero, excluyendo los 4 `.c` de la V3 |
| 6.466 líneas = 1 kB; 1.071.376 B = −2/0 kB; 15 clases = 44/45 kB | medición previa del propio proyecto, `assumptions.md` 2026-07-25/28, citada en `HANDOFF.md:159-161`. **[NO RE-MEDIDA en esta sesión]** |
| ~21 kB de arena al jubilar V3 | proyección: 7 clases × (44,5 kB / 15 clases). **[PROYECCIÓN, no medida]** |
| 217 clases en `4_World` | grep propio; **no casa** con el 199→185 que registró la palanca A2 — método de conteo posiblemente distinto. La ratio se usa como orden de magnitud |

## Lanes

| id | proveedor | binario | modelo_pin | tools | puede_leer_arbol | cuota_pre | output |
|---|---|---|---|---|---|---|---|
| `grok` | xAI vía Cursor | `%LOCALAPPDATA%\cursor-agent\cursor-agent.cmd` | `cursor-grok-4.6-xhigh` | `--mode ask` (solo lectura) | no (workspace aislado) | Ultra tarifa plana; verde 2026-09-07 00:12 (corrida g4 `result:success`) | `lanes/grok/council.jsonl` |
| `kimi` | Moonshot vía Cursor | idem | `kimi-k3-max` | `--mode ask` | no (workspace aislado) | Ultra tarifa plana; verde 2026-09-07 00:16 (corrida g3 `result:success`) | `lanes/kimi/council.jsonl` |
| `glm` | Zhipu vía Cursor | idem | `glm-5.2-max` | `--mode ask` | no (workspace aislado) | Ultra tarifa plana; catálogo confirmado 2026-09-07 00:05 (`--list-models`) | `lanes/glm/council.jsonl` |

**Lane que NO se nombra:** Codex/OpenAI. Pre-flight host-direct a las 2026-09-06 17:21 leyendo
`rate_limits` del rollout más reciente: `limit_id=codex`, `used_percent=100.0`,
`window_minutes=10080`. Al 100% de la ventana semanal, la lane no se nombra (regla C3). Es la lane
que G7 reserva para el juicio; su ausencia se declara aquí y se tiene en cuenta al arbitrar.

**Lane que NO se nombra (2):** Anthropic. El árbitro es Anthropic; meterla como lane haría
`arbitro=lane`. Además la ventana de la sesión ya tocó su límite una vez esta noche.

## Ceguera (regla C10)
- Workspace de cada lane: `ws/<id>/`, directorio **vacío** salvo una copia byte-idéntica de `BRIEF.md`. Sin acceso al árbol: la pregunta es de juicio sobre material ya verificado, no de verificación.
- Ningún prompt contiene la ruta de salida de otra lane.
- Las tres se lanzan **en paralelo**, no en cola.

## Verificación a la vuelta
- `modelo_pin` contra el oráculo: evento `{"type":"system","subtype":"init"}` del stream, campo `model` (nombre de display). Pin ≠ oráculo ⇒ se relabela la fila.
- Hash del input efectivo de cada lane == `brief_sha256`.
- Truncamiento: se comprueba que exista evento `result` con `subtype:success` y que la salida contenga las 6 secciones del contrato.
