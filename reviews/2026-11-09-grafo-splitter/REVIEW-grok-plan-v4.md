<!-- Salida literal de Grok (solo lectura, sesion nueva, a ciegas). Quitado solo el preambulo previo al veredicto. -->

APTO CON CAMBIOS

**R22v4-01 — IMPORTANTE — el vecino ruidoso en el mismo grafo se come el presupuesto y las filas nuevas de §6.4 no dan rojo/verde verdaderos**  
`scripts/5_Mission/LFPG_ElecGraphImpl.c:2248-2253`, `:2278-2290`, `:2786-2822`, `:2887-2921`; `scripts/3_Game/LFPG_Defines.c:465`, `:471`; PLAN §6.4 «Vecino ruidoso», §6.3 presupuestos, §9 GL-06.

El anillo XOR `output → input_1` cambia de señal cada pasada. Con `m_SignalChanged` en Step 3 vuelve a marcar aguas abajo y, si ya corrió en la época, `:2815` le resetea `m_LastEpoch`. En la misma cola están OR/XOR detrás de Splitter recorriendo la tabla. Tope **64 nodos / 256 aristas, cortado antes de tomar nodo** (`:2250-2253`).

Valores: anillo + relleno hasta 276 visitas (el banco) + OR/XOR A1-A3. Una llamada toma 64 nodos del anillo/relleno, deja OR/XOR en cola. Criterio: «OR y XOR siguen su tabla con como mucho un cambio por combinación» y «cola vacía en ≤100 ticks». Con el arreglo correcto el test sale **FAIL** (no asientan) o el arnés usa colas distintas y sale **verde sin morder GL-06**.

Arreglo: presupuesto **reservado** para el circuito bajo prueba (o tope de visitas del anillo por llamada); aserto de que OR/XOR **avanzan** N nodos/tick; el anillo en el mismo `ProcessDirtyQueue` y el mismo `m_EdgesVisitedThisEpoch`.

**R22v4-02 — IMPORTANTE — `signalOut = false` en todo `m_IsGated` sin entidad también apaga interruptores y Laser, y el PDQ directo tras rebuild corre con `m_NodeNetLow` vacío**  
`scripts/5_Mission/LFPG_ElecGraphImpl.c:2483-2503`, `:1683-1688`; `scripts/5_Mission/LFPG_NetworkManagerImpl.c:3714-3722`; `scripts/4_World/LFPG_SwitchDeviceBase.c:29`; `scripts/4_World/LFPG_LaserDetector.c:214`; PLAN §2.1 fila «cualquier nodo con compuerta», §3.1 Step 2b, §5.10.

`ResolveGatedEntity` es FindById → vanilla → NetworkID. Tras `RebuildFromWires` el mapa NetworkID **no existe** (`:1685-1688`). El warmup llama `ProcessDirtyQueue` en el mismo stack (`:3718-3722`).

Valores: swA on, swB on, AND 11, entidades vivas. `FindById` falla esa pasada (ref stale, el caso de `:3092-3096`). Vanilla no aplica. NetworkID miss. `m_IsGated` del interruptor → `signalOut=false`. AND lee `(0,1)` o `(0,0)`, publica `AND=false`. D5: Laser `m_IsGated`, misma miss → Set bajo aunque el Laser esté abierto. No es L-RACE: el proveedor **inventa baja** con el dispositivo vivo. G1 («nunca de una entidad sin resolver» = baja) choca con A1-A3/D5 en el PDQ de rebuild.

Arreglo: `signalOut=false` **solo** si el nodo es `LFPG_LogicGateBase` irresoluble; interruptor/Laser sin entidad conservan la tabla PASSTHROUGH + `m_GateClosed` cacheado (como la energía). Fixture: rebuild + `FindById` fail + mapas NetworkID vacíos + swA on.

**R22v4-03 — MENOR — Step 2b: entidad resuelta y `LFPG_LogicGateBase.Cast` nulo no entra en ninguna rama**  
PLAN §3.1 `:2516-2525`; `LFPG_ElecGraphImpl.c:2487-2503`.

Ramas escritas: Cast OK → `EvaluateGateLogic`; `m_IsGated` y **no** resuelta → `false`. Resuelta y Cast nulo (contador, o el AND si el `EntityAI` no es la clase script) deja `ComputeSignalOut` = `signalIn && !gateClosed`. AND 10: `signalIn` true (OR de Step 1 `:2308` previsto), publica **alto**. Tests S12/S14 no cubren este tercio.

Arreglo: `else if (node.m_IsGated && LFPG_IsLogicGateType(node)) signalOut = false`.

---

### Cierre de mis hallazgos de la v3 (§11.3)

| Id v3 | Decisión v4 | ¿Cierre correcto? |
|---|---|---|
| R22v3-01 BLOQUEA (`[AND,M,B]` Set falso) | L-RACE + G1/G2; se descarta orden topológico y Set que ignora alto no reescrito (§5.9) | **Sí como limitación de producto.** El fallo sigue en Enforce. No contradice el brief: A1-A3/D5/C1-C7 no meten dos cambios en la misma ventana. La frase de §5.9 («AND ya reescribió, el Set sordo no basta») es cierta para `[AND,M,B]`. |
| R22v3-02 IMPORTANTE (entidad null → latch cacheado) | tres niveles + baja sin entidad | **A medias.** Cierra el AND con `FindById` fail y NetworkID acierto, y el fail-closed del AND. Amplía la baja a **todo** `m_IsGated` (R22v4-02). |
| R22v3-03 IMPORTANTE (barrido asienta M) | sin cambio en producción; barrido solo con cola vacía | **Sí.** `:2220-2231` y `:2913-2921`: con AND/B pendientes no corre. Con cola vacía el alto de L-RACE ya lo reescribió AND. Set de nivel no baja el latch; el aserto «el barrido no cambia el latch» es el comportamiento real. |
| R22v3-04 MENOR (ancla i / `inputSum`) | `inputChanged` apagado e `inputSum` 0→0 en swA | **Sí.** |

---

### Las cinco preguntas (§10)

1. **¿Camino G1 (valor que el proveedor no calculó con lo que leía)?** Sí: R22v4-02 (baja inventada con dispositivo vivo e irresoluble). L-RACE no rompe G1 tal como lo reescribe la v4 (AND escribe `AND` de bits **ya publicados**). `SetOutputPortEnabled` a false está en G1. PDQ directo: mismo pre-gate.

2. **¿G2 con cola y diferidos vacíos y entidades resueltas?** Con entidades resueltas y el bit solo en `AllocateOutput` / disable, no he visto arista habilitada distinta del oráculo. Tras L-RACE la arista AND→Set queda baja y G2 puede cumplir con M **true** (el oráculo es de aristas, no del latch de M). El invariante alloc>EPS ⇒ señal no ve L-RACE; el oráculo sí ve las aristas.

3. **¿L-RACE contradice brief o Nostra?** No. Brief: oscilación A1-A4, D5 Laser→Set nunca alto, C1-C7 pulsadores. Nostra no cablea Set en el mismo tick que un flanco de entrada. XOR de escalera asienta entre transiciones (`§6.3` cola vacía).

4. **¿Tres niveles dan baja con entidad viva? ¿Energía §5.10?** Sí, R22v4-02. Energía: si NetworkID acierta, `gateEntityResolved` pasa a true (`:2642-2690`) y un `m_GateClosed` cacheado distinto del vivo dispara `m_AllocChanged` y suciedad aguas arriba — el surge/apagón que `:2499-2502` quería evitar. Con registro sano no cambia. No rompe el all-off B5.

5. **¿Filas/reglas nuevas dan rojo/verde verdaderos?** Carrera (a) vs L-RACE: sí si el bit no se escribe al encolar (mutante nuevo). Cambios separados por vaciado: sí. Entidad FindById/NetworkID: sí. Entidad irresoluble: no cubre interruptor/Laser (R22v4-02). §5.8 con Set a 0 en Reset: sí sobre el texto de Memory Cell `:235-242`. C con congestión de 65 CONSUMER: ~2 PDQ, pulso 2000 ms, verde creíble. **Vecino ruidoso: no** (R22v4-01). Barrido: «con nodos en cola no corre» es cierto **durante** la llamada; al **drenar** sí corre (`:2914-2921`). Un aserto de «esta llamada empezó con cola ⇒ cero barrido» sería falso negativo.

---

### Premisa de esta v4 que puede estar mal

G1 se redefine para **excluir** el BLOQUEA de la v3 (entradas aún no reescritas en la época). Con eso L-RACE deja de romper G1 y el veredicto pasa a limitación. Es una decisión de producto, no un cierre técnico del escenario `[AND,M,B]`.

«El alto pasajero solo existe mientras la puerta sigue en cola» (§5.11): cierto si B sigue pendiente. Si AND y M ya salieron y B **no** estaba sucio, no es L-RACE (B sigue en 1).

---

### Qué defecto no verían los tests del apartado 6

- Vecino ruidoso con colas separadas o sin compartir `m_EdgesVisitedThisEpoch` (R22v4-01).
- Rebuild + NetworkID map vacío + `FindById` fail en interruptor/Laser (R22v4-02): los fixtures de entidad solo tumban la **puerta**.
- `Cast` nulo con entidad resuelta (R22v4-03).
- Modelo que drena `CallLater(50)` y PDQ en el mismo ms **sin** probar los dos órdenes: el plan lo pide; si el arnés no lo cumple, Q→Toggle verde y en partida no.
- Linter `AGENTS.md` §1: no ve `allocEdge.m_SignalHigh` si el campo no está en `LFPG_ElecEdge`.
- L-RACE no se juega en fase 3 (§8).

---

### Comprobado / no comprobado

**Comprobado (lectura en este árbol, código = `main` 592ebcc):** `PLAN-CONSOLIDADO.md` §0, §2.1, §3.1, §5.9-5.11, §6.3-6.6, §8-11.3 vs `PLAN-CONSOLIDADO-v3.md`; `REVIEW-grok-plan-v3.md`; `BRIEF-GRAFO.md` síntomas 1-3 y criterios; `AGENTS.md` §1; `LFPG_ElecGraphImpl.c` PDQ `:2219-2354`, SOURCE/PT `:2366-2407`, pre-gate `:2473-2525`, `m_GateClosed` `:2635-2710`, Step 3 `:2725-2921`, `NodeEntitySyncUnchanged` `:2966-2986`, `SyncNodeToEntity` `:3044-3133`, `ValidateConsumerStates` `:3417-3587`, `AllocateOutput` `:4562` y Pass 2 `:4749-4798`, `IsPortReceivingPower` `:5013-5048`, `SetOutputPortEnabled` `:5053-5106`, NetworkID `:1382-1392` y nota `:1683-1688`; `LFPG_NetworkManagerImpl.c:3154-3158`, `:3714-3722`; `LFPG_LogicGate.c:119-220`; `LFPG_MemoryCell.c:142`, `:197-270`; `LFPG_SwitchDeviceBase.c:29`; `LFPG_LaserDetector.c:214`; `LFPG_IDevice.c:561-576`.

**No comprobado:** ejecutar tests; Enforce in-game; que `FindById` falle de verdad tras rebuild (R22v4-02 es el caso que tumba `:3092-3096` + mapas vacíos); `GRAFO-SPLITTER-MEDICIONES.md`; `enforce_scalar_slice`; `checks.yml` línea a línea; la revisión de Codex (prohibida). El orden de cola es el del PDQ leído, no una traza de partida.
