<!-- Salida literal de Grok (solo lectura, sesion nueva, a ciegas). Quitado solo el preambulo previo al veredicto. -->

NO APTO

**R22v5-01 — BLOQUEA — el reintento encola con `LFPG_DIRTY_INTERNAL` y Step 5 se salta `SetPowered`; el latch no vuelve a la tabla**  
`scripts/5_Mission/LFPG_ElecGraphImpl.c:2079-2121`, `:2303-2306`, `:2966-3029`, `:3115-3133`; PLAN §3.1 `RetryUnresolvedGates` + `MarkNodeDirty(..., LFPG_DIRTY_INTERNAL)`, §2.1 G2, §5.10, §6.4 «Entidad irresoluble y su vuelta».

`NodeEntitySyncUnchanged` solo obliga a escribir con `DIRTY_TOPOLOGY` o `DIRTY_INPUT` (`:2982-2986`). `DIRTY_INTERNAL` no está. Si `m_Powered` y `m_Overloaded` coinciden con el último sync, `:3133` vuelve sin callback. `skipInputEval` con INTERNAL es solo para SOURCE (`:2303`); una puerta PASSTHROUGH sí recalcula `signalOut` y escribe aristas.

Valores: AND abierto en 11, `m_Powered=true` ya sincronizado. Pasa a 10 y los tres niveles fallan: Step 2b pone `signalOut=false` y `QueueGateRetry`; Step 5 sale en `:3115` sin `SetPowered`; `m_GateOpen` sigue true. Vuelve solo el registro, cola vacía. `RetryUnresolvedGates` hace `MarkNodeDirty(AND, INTERNAL)`. Siguiente PDQ: `EvaluateGateLogic(1,0)=false`, arista ya baja (`m_SignalChanged` apagado), `lastPowered==m_Powered`. Callback no corre. G2: latch de la puerta = tabla → AND 10 abierto. Bajo sigue bajo; M no se activa; el test de «A→B alta y B abierto» no mira este latch.

Arreglo: `MarkNodeDirty(nodeId, LFPG_DIRTY_INPUT)` (o INTERNAL|INPUT), o tratar INTERNAL como «hay que escribir» en `NodeEntitySyncUnchanged`. El fixture de vuelta tiene que leer el latch de la puerta que se recuperó, no solo las aristas aguas abajo.

**R22v5-02 — IMPORTANTE — reintento eterno en vacío: 4 resoluciones por PDQ ocioso sin tope de lista**  
`LFPG_ElecGraphImpl.c:2220-2237`, `:2913-2921`; `LFPG_Defines.c:510-514`; PLAN §5.10, §3.1 `RetryUnresolvedGates`, pregunta 4.

Con n ids que no resuelven nunca, cada llamada en vacío (las de `:2231` que hacen `return 0`) prueba 4 y las rota. No encola. `ValidateConsumerStates` solo cada 10 ticks; el reintento corre en todas las ociosas. Dispositivo borrado con nodo aún en `m_Nodes` (`m_IsGated` true): `FindById` + vanilla + `GetObjectByNetworkId` ×4 cada 100 ms hasta que alguien quite el nodo.

Valores: 1 interruptor huérfano en `m_GateRetry`. 10 Hz × 4 lookups = 40 resoluciones/s con cola vacía. El tope 4 acota el pico, no el tiempo. El barrido «igual que sin reintentos» deja de ser cierto en el tick en que uno vuelve: esa llamada encola y la siguiente ya no está vacía al empezar.

Arreglo: tope de edad o de vueltas (p. ej. 50 rotaciones → quitar y GL-08); o reintentar con el mismo intervalo que el barrido. En el fixture de 10+1, afirmar también las llamadas ociosas *después* de recuperar.

**R22v5-03 — MENOR — `m_IsLogicGate` no se refresca si el nodo ya existía**  
`LFPG_ElecGraphImpl.c:1368-1372`, `:1414`; PLAN §3.1 EnsureNode / Populate.

`EnsureNode` si el id ya está en `m_Nodes` vuelve sin tocar flags. `m_IsLogicGate` solo nace en el `new` y en Populate. Un AND cuyo primer `EnsureNode` llegó con `obj` que no casteaba a `LFPG_LogicGateBase` (o sin `obj`) queda `m_IsLogicGate=false` y Step 2b usa PASSTHROUGH: AND 10 con `signalIn` true publica alto. Populate en rebuild sí lo corrige; un alta de cable en caliente no.

Arreglo: el mismo `Cast` al pasar por EnsureNode aunque el nodo exista, o al resolver en el pre-gate si `m_IsGated`.

---

### Cierre de mis hallazgos de la v4 (§11.4)

| Id v4 | Decisión v5 | ¿Cierre correcto? |
|---|---|---|
| R22v4-01 IMPORTANTE (vecino ruidoso / anillo XOR) | Sin bucle; ruido externo 500 ms + 65 CONSUMER; misma cola y contador; asiento en ≤5 ticks; no reservar presupuesto | **Sí.** `:2250-2253` corta antes de tomar nodo; el juego rechaza ciclo (`OnWireAdded` `:543-544`, RPC `:509`). 65 pendientes caben en 2 PDQ; 5 ticks (500 ms) da margen si el arnés comparte cola. El anillo de v4 ya no es el fixture. |
| R22v4-02 IMPORTANTE (baja en todo `m_IsGated`) | Baja solo si `m_IsLogicGate`; resto cacheado + retry | **Sí en el contrato.** Interruptor/Laser irresolubles no inventan baja. Matiz de NetworkID: `:289` limpia y `EnsureNode` `:1382-1392` vuelve a llenar en el rebuild; la nota `:1685-1688` es de huérfanos. La fila sin NetworkID sigue haciendo falta. El retry con INTERNAL (R22v5-01) no deshace este cierre de señal. |
| R22v4-03 MENOR (`Cast` nulo) | `m_IsLogicGate` + baja + retry | **Sí** si el flag es true. Si el flag nació mal (R22v5-03), la rama no corre. |

---

### Las cinco preguntas (§10)

1. **¿Cierra el reintento el escenario «vuelve solo el registro»?** Las aristas de salida sí, en la PDQ *siguiente* a la ociosa que resolvió (vacío en `:2231` hace `return 0` tras `MarkNodeDirty`). No encola si `Resolve` falla. **No cierra el latch** (R22v5-01). Camino sin reproceso: nodo gated que nunca entró en `m_GateRetry` (no `m_IsGated`, o rebuild que vacía la lista y el warmup resuelve). Cola ocupada: la entidad puede volver y no se prueba hasta el vacío, como el barrido; G2 no aplica hasta entonces.

2. **¿Baja solo en puertas vs cache en el resto rompe G1?** G1 v5 admite cache en no-puertas y baja en puertas sin entidad. Señal vs energía en puerta irresoluble (baja / cache) está declarado. Tras recuperar, energía puede corregirse por `:2688-2706` si el vivo ≠ cache; señal de interruptor no debería bajar. G1 no cubre el latch de R22v5-01 (eso es G2).

3. **¿Step 2b cubre los casos? ¿`m_IsLogicGate` en todos los nacimientos?** Cubiertos en texto: lógica resuelta / Cast nulo / sin entidad; gated no-lógica; no gated. Nacimiento: EnsureNode (rama `obj` PASSTHROUGH) y Populate. Falta el early-return de EnsureNode (R22v5-03). No hay tercer sitio que ponga `m_IsGated` (grep: `:1414` y `:3705`).

4. **¿Tráfico permanente, barrido, reencolados, borrados?** Tráfico de resolución sí (R22v5-02). Barrido: retry *después* del barrido y sin encolar si no resuelve → el barrido ve lo mismo *mientras no hay éxitos*. Un éxito encola y desplaza el siguiente vacío. Reencolados: INTERNAL en la PDQ *siguiente* es época nueva; `EnsureRequeueEpoch` (`:4984-4996`) pone `m_RequeueCount=0`. Borrados: quita si no hay nodo; si el nodo sigue, la entrada vive.

5. **¿Vecino ruidoso y filas nuevas dan rojo/verde de verdad?** Vecino: sí, si ruido y circuito comparten `m_DirtyQueue` y `m_EdgesVisitedThisEpoch` y el MCP conmuta de verdad; 5 ticks es justo el periodo del ruido. Entidad / Cast / retry acotado: el de vuelta queda **verde con latch podrido** si no asertan `m_GateOpen` (R22v5-01). Retry acotado muerde tope y rotación; no muerde lookups eternos.

---

### Premisa de esta v5 que puede estar mal

Que `MarkNodeDirty(INTERNAL)` «reprocesa el nodo» como una pasada que actualiza dispositivo. INTERNAL está pensado para SOURCE (`:2303`) y para no forzar `DIRTY_INPUT`. G2 habla de latch = tabla; el diseño trata el reproceso como si Step 5 corriera siempre.

Que G2 con «ningún pendiente de reintento» basta: el pendiente se quita *antes* de procesar, en la llamada ociosa. G2 se evalúa tras la siguiente PDQ, donde el skip de sync ya ocurrió.

---

### Qué defecto no verían los tests del apartado 6

- Latch AND 10 abierto tras retry INTERNAL (R22v5-01) si la fila solo mira aristas y B.
- Lookups eternos con cola vacía (R22v5-02): el de 10+1 para el ruido a los 3 vacíos y no sigue midiendo.
- `m_IsLogicGate` falso en EnsureNode existente (R22v5-03).
- Vecino con colas o presupuestos separados (sigue el mutante de arnés; ahora el plan lo pide).
- Linter `AGENTS.md` §1: no ve `m_IsLogicGate` mal puesto si el campo existe.
- L-RACE fuera de fase 3.

---

### Comprobado / no comprobado

**Comprobado (lectura, código = este worktree = `main` 592ebcc para `scripts`):** PLAN v5 §0, §2.1, §3.1, §4, §5.10-5.11, §6.3-6.7, §7-11.4 vs v4; `REVIEW-grok-plan-v4.md`; `AGENTS.md` §1; PDQ `:2219-2237`, `:2248-2292`, `:2303`, `:2483-2525`, `:2638-2710`, `:2786-2921`; `MarkNodeDirty` `:2079-2121`; `EnsureRequeueEpoch` `:4984`; `NodeEntitySyncUnchanged` `:2966-3029`; `SyncNodeToEntity` `:3044-3133`; `EnsureNode` `:1368-1427`; `RebuildFromWires` `:281-330`; NetworkID `:1382-1392`; Populate `:3705`; warmup NM `:3714-3722`; `OnWireAdded` `:543-544`; `DetectCycleIfAdded` `:1172`; RPC ciclo `:509`; Defines 465/471/494/455/514/518; `LFPG_LogicGate.c:112-145`; `IsGateCapable` `LFPG_IDevice.c:570-576`; Switch/Laser gated; Memory Cell `IsGateCapable` false.

**No comprobado:** ejecutar tests; Enforce in-game; que `FindById` falle de verdad; `GRAFO-SPLITTER-MEDICIONES.md`; revisión de Codex (prohibida). El orden de cola es el del PDQ leído.
