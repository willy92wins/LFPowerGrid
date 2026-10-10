# SPEC — G-01 / issue #76: reparto multifuente del grafo

Lane i76. Ronda de spec; no hay PR de codigo en esta ronda.
Base leida: worktree `wt-i76` (commit de rama = `463464e` / `main` segun brief).
Las citas `path:line` son del fichero actual localizado **por contenido**.

**Decision conservadora (G1 cerrado por el brief):** el cierre del issue pide
`20+30` para demanda hard 50 con fuentes 20 y 50. Eso no es un split
proporcional a capacidad nominal (`20/70*50 ≈ 14.29 + 35.71`). La regla
elegida es **reparto igual entre proveedores alimentados, tope por
capacidad residual del edge, y redistribucion del resto**. Motivo: es el
cambio local minimo sobre el divisor actual (`edgeDemand / ptPoweredIn` en
`LFPG_ElecGraphImpl.c:4023-4028`) y produce exactamente `20+30`.

---

## 1. Mecanismo actual

### 1.1 Quien llama y con que `availableOutput`

`ProcessDirtyQueue` procesa SOURCE y PASSTHROUGH en el paso 2b
(`LFPG_ElecGraphImpl.c:2452-2507`).

- SOURCE encendida: `newOutput = node.m_MaxOutput` (`:2348-2354`).
- Antes de `AllocateOutput`, el SOURCE publica `node.m_OutputPower = newOutput`
  (`:2488-2498`) para que `CountPoweredIncoming` vea la fuente en el mismo
  epoch (comentario v2.2 Bug #2).
- PASSTHROUGH: `newOutput` es el throughput tras autoconsumo y cap
  `m_MaxOutput` (`:2356-2407`); **no** se publica `m_OutputPower` antes de
  asignar.
- Gate cerrado: `allocAvail = 0` (`:2501-2505`). Combiner/Splitter no son
  gated (`LFPG_Combiner.c` no implementa gate; `m_IsGated` queda false).
- Llamada: `downstreamDemand = AllocateOutput(nodeId, allocAvail)` (`:2507`).

### 1.2 Estimacion de demanda por edge (Pass 1)

Bucle de salidas en `AllocateOutput` (`:3930-4043`). Solo edges con
`LFPG_EDGE_ENABLED`.

| Tipo del target | Demanda |
|---|---|
| CONSUMER / CAMERA | `targetNode.m_Consumption` (`:3944-3946`) |
| PASSTHROUGH | `targetNode.m_LastStableOutput` (`:3949-3950`) |

**Cold start v2.4** (PASSTHROUGH, `m_LastStableOutput < epsilon` y
`!m_DemandKnown`, `:3953-4020`):

1. Gate cerrado (`m_GateClosed`): `max(m_Consumption, LFPG_GATE_PROBE_DEMAND)`
   (`:3979-3994`). `LFPG_GATE_PROBE_DEMAND = 1.0` (`LFPG_Defines.c:504`).
2. Gate abierto / no gated:
   - Si hay outgoing enabled (`HasEnabledDownstream`, `:3803-3838`) y
     `m_MaxOutput > epsilon`: estima `m_MaxOutput`, **luego tope a
     `availableOutput`** (`:4000-4014`). Ese tope es el v2.4 (bateria /
     overload falso en epoch 1).
   - Si no hay downstream: `m_Consumption` (`:4016-4019`). Combiner vacio
     pide 0 (`LFPG_Combiner.c:67`).

`m_DemandKnown` se pone true tras publicar hard/soft en el PASSTHROUGH
(`:2695-2703`). Un cero publicado es demanda real; ya no se reestima.

**Divisor multifuente** (`:4023-4028`):

```
ptPoweredIn = CountPoweredIncoming(edge.m_TargetNodeId);
if (ptPoweredIn > 1)
    edgeDemand = edgeDemand / ptPoweredIn;
```

`CountPoweredIncoming` (`:3845-3901`): cuenta incoming enabled cuyo
proveedor tiene `supplierPower > LFPG_PROPAGATION_EPSILON` (`Defines.c:497`).

- SOURCE: `supplierPower = m_OutputPower` (capacidad publicada, **aunque
  `m_Overloaded` sea true**).
- PASSTHROUGH: `m_InputPower + m_VirtualGeneration - m_Consumption`, 0 si
  gate cerrado (`:3876-3889`).

Memo por epoch; `AllocateOutput` **borra** `m_PoweredIncomingMemo` al entrar
(`:3910-3911`), asi que el recuento es fresco en cada asignacion.

Porcion soft: `edgeSoftPortion = edgeDemand * m_SoftDemandRatio` si el ratio
`> epsilon` (`:4033-4036`). Combiner deja ratio 0.

### 1.3 Overload solo-hard, Pass 2 y Pass 3

`totalHardDemand = totalDemand - totalSoftDemand` (`:4049-4056`).
Overload si `totalHardDemand > availableOutput + epsilon` (`:4058-4061`).
Politica **all-off**: si `overloaded`, Pass 2 pone `newAlloc = 0` en **todas**
las salidas (`:4086-4110`). Soft **no** provoca overload.

Pass 2 si no hay overload:

- Con soft: asigna solo hard `m_Demand * (1 - ratio)` (`:4088-4103`).
- Sin soft: `newAlloc = m_Demand` (`:4107-4108`).

Pass 3 (`:4124-4168`): si no overload y hay soft, reparte
`min(available - totalAllocated, totalSoftDemand)` proporcional a la
porcion soft de cada edge.

LoadRatio / `m_Overloaded` se escriben en el nodo fuente (`:4199-4238`).
Retorno: `totalDemand` hard+soft (`:4242-4244`).

### 1.4 Lectura aguas abajo: `GetEdgeAllocatedPower`

`:4253-4302`. Si el origen esta `m_Overloaded` → 0 (`:4263-4265`). Si
`m_AllocatedPower > epsilon` → ese valor. PASSTHROUGH origen con alloc 0 →
0 (sin fallback). SOURCE con alloc 0: split igual de `m_OutputPower` entre
salidas enabled (`:4289-4298`) — solo cold-start de SOURCE.

El Combiner suma esas lecturas en Step 1 (`:2307-2335`).

### 1.5 Recuperacion de un overload hoy

Un SOURCE sobrecargado **sigue** con `m_OutputPower = m_MaxOutput` (se
publico antes de asignar y `newOutput` no baja). `CountPoweredIncoming`
**sigue contandolo**. El divisor sigue pidiendo `D/N`. Si `D/N` supera a la
fuente chica, esa fuente **permanece** en overload y `GetEdgeAllocatedPower`
devuelve 0.

Salidas de overload que si existen:

- Fuente apagada: `newOutput < epsilon` limpia `m_Overloaded` (`:2509-2515`).
- Demanda hard del SOURCE cae a `<= availableOutput` (p. ej. se retira la
  carga): Pass 2 asigna de nuevo; `m_AllocChanged` reencola aguas abajo
  (`:2759-2804`).
- Cambio de `inputSum` en un PASSTHROUGH con demanda estable: `inputChanged`
  (`:2712-2732`) reencola upstream (`:2808-2851`) — pensado para
  “un SOURCE de N salio de overload” (comentario v2.2 Bug #1).

`EnsureRequeueEpoch` (`:4305-4318`) pone `m_RequeueCount = 0` al cambiar de
epoch. Tope `LFPG_MAX_REQUEUE_PER_EPOCH = 5` (`Defines.c:468`).

**Consecuencia para el contraejemplo 20+50→50:** la fuente 20 no deja de
contar como powered, el split sigue en 25+25, y no hay recuperacion
espontanea. Excluir sobrecargadas del recuento tampoco basta: la de 20
desapareceria, la de 50 pasaria a pedir 50 y serviria 50, pero al
recuperar la de 20 el split volveria a 25 y la de 20 caeria otra vez
(oscilacion). Ponderar solo por capacidad nominal (`20:50`) da
`≈14.29+35.71`, que es factible para D=50 pero **no** es el `20+30` del
issue y no demuestra que la fuente chica se sature sin pedir de mas.

---

## 2. Contraejemplos

Notacion: S20 / S50 = SOURCE `m_MaxOutput` 20 / 50, un outgoing cada una
salvo donde se diga. C = Combiner PASSTHROUGH cap 500, consumo 0
(`LFPG_Combiner.c:66-68`). epsilon = 0.001. Demandas ya publicadas
(`m_DemandKnown=true`) salvo (e).

### 2.a 20 + 50 → Combiner → hard 50

**Hoy (estable, LastStableOutput de C = 50):**

1. S20 y S50 publican `m_OutputPower` 20 y 50.
2. `CountPoweredIncoming(C) = 2`.
3. Cada SOURCE ve `edgeDemand = 50/2 = 25`.
4. S20: `25 > 20+eps` → overload, alloc 0.
5. S50: `25 <= 50` → alloc 25.
6. C recibe `GetEdgeAllocatedPower`: 0 + 25 = 25. El consumidor 50 no
   cubre consumo → apagado.

**Exigido:** 20 + 30. Conservacion: 20+30=50. Ninguna fuente pide mas que
su residual. C transmite 50.

### 2.b Deficit real: 20 + 50 → Combiner → hard 80

Suma de residuales = 70 < 80.

**Hoy:** split 40+40. S20 overload (0). S50 alloc 40. C ve 40. Ademas, si
C ya publico 80 como `m_LastStableOutput`, **C** hace
`totalHardDemand=80 > available(40)` → **all-off en C** y el consumidor
ve 0. Peor que el deficit fisico.

**Politica SOURCE (no se cambia):** all-off si *su* hard pedido supera *su*
`availableOutput`. Un SOURCE sola con demanda 80 y cap 50 sigue a 0.

**Politica PASSTHROUGH merger (cambio justificado):** un Combiner / splitter
de fusion **no es generador**. All-off cuando el downstream pide mas que
el input destruye la energia ya asignada por las fuentes. Exigido:

- Pedido a cada SOURCE = `min(residual_e, restante)` tras cap-and-fill →
  20 y 50. Ninguna SOURCE sobrecarga.
- C asigna aguas abajo `min(demanda hard, availableOutput)` = 70, **sin**
  poner sus edges a 0. `m_Overloaded` de C puede quedar true como
  telemetria (load > 1) **o** calcularse como `totalAllocated/capacity`
  sin all-off; la implementacion debe documentar el bit. Recomendacion:
  `m_Overloaded=true` en C (deficit visible) y alloc = available, no 0.

**Exigido numerico:** alloc S20=20, S50=50, C→load=70, consumidor unpowered
(70 < 80). Motivo: maxima entrega factible; overload de SOURCE solo cuando
ese SOURCE es el cuello.

### 2.c Fuente compartida (splitter + Combiner)

Topologia:

- S50 → Splitter Sp (3 OUT, cap 200, `LFPG_Splitter.c:52-70`).
- Sp OUT1 → isla ajena: CONSUMER hard 10 (p. ej. `LFPG_LampDeviceBase`
  consumo 10, `:38-39`).
- Sp OUT2 → Combiner C input_1.
- S20 → C input_2.
- C → CONSUMER hard 50 (`LFPG_WaterPump` consumo
  `LFPG_PUMP_CONSUMPTION=50`, `Defines.c:709`).
- Isla Z (otro `m_ComponentId`): S_default 50 (`LFPG_DEFAULT_SOURCE_CAPACITY`,
  `Defines.c:183`) → otra bomba. No comparte edges.

Residual de S50 hacia C: `50 - 10 = 40` (la otra rama de Sp se lleva 10
hard). Residual S20: 20. Demanda C: 50. Cap-and-fill: min(25,20)+min(25,40)
= 20+25, resto 5 → 20+30 hacia C. Sp pide 10+30=40 a S50 ≤ 50.

**Hoy:** C pide 25 a cada incoming. Sp pide 25 (hacia C) + 10 (lampara) = 35
a S50. S20 pide 25 → overload 0. C recibe ~25 (si Sp entrega) + 0.

**Exigido:** S20=20, rama C de Sp=30, lampara=10, S50 no overload.
Isla Z: mismo `componentId`, misma potencia. Localidad.

### 2.d Hard + soft mezclados

S20 + S50 → C → (a) bomba hard 50 y (b) bateria en carga con
`m_SoftDemandRatio` en el PASSTHROUGH de bateria.

Modelo en el edge C→bateria: demanda total D_batt, ratio r
(`LFPG_ElecNode.m_SoftDemandRatio`, `LFPG_Data.c:258-264`). Soft no entra
en `totalHardDemand` (`AllocateOutput:4049-4060`).

Ejemplo estable: C sale a bomba 50 hard y a bateria que pide 20 de los
cuales 20 son soft (ratio 1.0) y 0 hard. Demanda hard de C = 50.
Reparto entre S20/S50 como (a): 20+30. Surplus de S50 = 20, que Pass 3
puede mandar a la rama soft **en el SOURCE que tenga surplus**. S20 esta
saturado (residual 0); el soft sale de S50.

**Hoy:** split hard 25+25, S20 overload 0, S50 da 25 hard; soft de S50
Pass 3 con surplus 25. Bomba hambrienta.

**Exigido:** hard 20+30 primero; soft solo con surplus; soft nunca dispara
overload. Ratio se propaga por cadenas PASSTHROUGH como hoy (`:2539-2595`).

### 2.e Cold start (primer epoch, `!m_DemandKnown`, `m_LastStableOutput=0`)

C cap 500, hay downstream enabled, S20 `availableOutput=20`.

**Hoy por fuente:** estima `min(500, availableOutput)` luego `/2` si ambas
ya publicaron output. S20: `min(500,20)/2 = 10`. S50: `min(500,50)/2 = 25`.
Ninguna overload. C recibe 10+25=35. Luego C publica demanda real 50
(`m_DemandKnown=true`) y el epoch 2 cae en (a) y se rompe.

**Exigido:** epoch 1 no debe sobrecargar (conservar tope v2.4). Epoch 2+
converge a 20+30. Cota: ver §3.5.

### 2.f Recuperacion tras retirar la carga que sobrecargaba

Estado inicial = (a) roto: S20 overload, S50=25. Se desconecta o se apaga
el consumidor 50 (demanda C → 0).

**Hoy:** C publica 0; split 0; S20 `totalHard=0` → sale de overload en el
epoch en que reasigna. Cables IDLE. 1 epoch de PDQ tras el dirty de C +
requeue upstream (`:2808-2848`).

**Exigido con el algoritmo nuevo:** mismo o mejor. Si en vez de retirar se
baja de 50 a 30: split igual 15+15, ambas bajo residual, 1 epoch.
Si se deja 50: con el algoritmo nuevo S20 **no deberia haber estado** en
overload; recuperacion = 0 epochs extra respecto al estable 20+30.

Cota: ≤ 2 epochs de PDQ tras el dirty (C publica, fuentes reasignan),
acotado por `LFPG_MAX_REQUEUE_PER_EPOCH=5`.

### 2.g Fuente en overload por OTRA rama, alimentando el Combiner

S50 tiene OUT1 → consumidor hard 60 (all-off en S50) y OUT2 → C junto a S20,
C pide 50.

**Hoy (politica all-off SOURCE, se conserva):** S50 `60+25 > 50` → **todas**
las salidas a 0, incluida la rama del Combiner. C solo puede recibir S20,
que pide 25 y overload 0. Red a 0.

**Exigido (conservador, no se relaja el all-off de SOURCE):** S50 sigue
all-off mientras *su* hard total (60 + pedido hacia C) > 50. El pedido
hacia C debe ser `min(residual, …)` con residual = `available - otras
asignaciones hard`. Residual de S50 hacia C = `50-60` clamp 0. C pide 0 a
S50 y todo lo servible a S20: `min(50,20)=20`. S20 no overload. C entrega
20. Isla de S50 (consumidor 60) sigue a 0 por all-off — es la politica
vigente, no G-01.

Si el consumidor 60 se apaga: residual S50→C vuelve a 50 y se reentra en
(a) exigido 20+30 en ≤ 2 epochs.

---

## 3. Algoritmo propuesto

### 3.1 Donde vive

Solo cuando el **target** del edge es PASSTHROUGH y
`CountPoweredIncoming(target) > 1` (hoy `:4023-4028`). El 99% de redes
tienen 1 incoming: la rama no se toca (invariante I4).

Combiner de produccion: 2 IN (`LFPG_Combiner.c:53-56`). El nucleo
comprobable es binario; N>2 (si un PASSTHROUGH tuviera mas IN) usa el
mismo cap-and-fill en el bucle existente de Pass 1.

### 3.2 Capacidad residual por edge

Para el edge E de proveedor P hacia merger T, en el epoch actual, **antes**
de escribir `E.m_AllocatedPower`:

```
residual(E) = availableOutput(P) - sum_{F salida enabled de P, F != E} hard(F)
```

`hard(F)` = demanda hard ya anotada en Pass 1 para F si F ya se visito en
este mismo `AllocateOutput`; si no, `F.m_AllocatedPower` del epoch previo
capado a lo que F tendria de hard (`m_Demand * (1-ratio)`).

Clamp: si `residual < 0` → 0.

SOURCE de un solo outgoing: `residual = availableOutput = m_MaxOutput`.
PASSTHROUGH proveedor: `availableOutput` es el `allocAvail` que PDQ ya
paso (throughput real).

**No** usar `m_MaxOutput` del Combiner (500) como residual de las fuentes.

### 3.3 Regla de pedido (hard)

Nucleo de 2 proveedores (el Combiner). Pseudocodigo estilo Enforce:

```
protected float SplitTwoProviderHard(float demand, float residualA, float residualB)
{
	float eps = LFPG_PROPAGATION_EPSILON;
	if (demand < eps)
		return 0.0;
	float capA = residualA;
	if (capA < 0.0)
		capA = 0.0;
	float capB = residualB;
	if (capB < 0.0)
		capB = 0.0;
	float sumCap = capA + capB;
	if (sumCap < eps)
		return 0.0;

	float share = demand / 2.0;
	float askA = share;
	if (askA > capA)
		askA = capA;
	float askB = share;
	if (askB > capB)
		askB = capB;

	float leftover = demand - askA - askB;
	if (leftover < 0.0)
		leftover = 0.0;

	float roomA = capA - askA;
	float roomB = capB - askB;
	if (leftover > eps)
	{
		if (roomA > eps)
		{
			float add = leftover;
			if (add > roomA)
				add = roomA;
			askA = askA + add;
			leftover = leftover - add;
		}
	}
	if (leftover > eps)
	{
		if (roomB > eps)
		{
			float add2 = leftover;
			if (add2 > roomB)
				add2 = roomB;
			askB = askB + add2;
			leftover = leftover - add2;
		}
	}
	return askA;
}
```

El llamador obtiene `askB = min(demand - askA, capB)` (o un segundo return
via out-params en Enforce: dos llamadas simetricas `SplitTwoProviderHard`
con A/B intercambiados deben cumplir `askA+askB = min(demand, capA+capB)`).

Para N incoming en Pass 1: misma idea en el bucle ya existente (share =
`demand/N`, tope residual, segundo barrido de leftover). Coste O(K) con
K = fan-in (ya se recorre incoming en `CountPoweredIncoming`).

Cold start: `demand` sigue siendo la estima v2.4 **antes** del split;
despues se aplica cap-and-fill. S20+S50 epoch 1: estima por fuente
`min(500, avail)` no se usa como demanda del merger; la demanda del merger
es unica (LastStable o estima del **target**). Correcto: calcular
`edgeDemand` del target **una vez** (LastStable o cold-start **sin** tope
al `availableOutput` del llamador, porque ese tope es asimetrico por
fuente), luego split por residual.

**Decision (cold-start multifuente):** el tope v2.4 `if (edgeDemand > availableOutput) edgeDemand = availableOutput` (`:4011-4014`) se aplica
**despues** del split, por fuente (`ask = min(ask, availableOutput)`), no
antes. Motivo: topar a 20 y luego dividir por 2 pide 10 a S20 y, en S50,
topar a 50 y dividir pide 25: asimetrico. Demanda del target = LastStable
o `m_MaxOutput` del Combiner (500) es inutilmente grande; para cold start
multifuente usar `min(target.m_MaxOutput, suma de residuales conocidos)`
si residuales > 0, si no `LFPG_DEFAULT_SOURCE_CAPACITY` (50) como cota
de bootstrap. Conservador: **demanda bootstrap = min(target.m_MaxOutput,
sumResidual)` si sumResidual>eps, else `availableOutput` del llamador
(v2.4 clasico, un proveedor).**

### 3.4 Prioridad hard sobre soft

Sin cambio de Pass 2/3: overload mira solo hard; Pass 3 solo surplus.
El split residual aplica a la **demanda hard del merger**
(`LastStable * (1-ratio)` + consumo propio del merger). Soft del merger
se senala como hoy en `m_SoftDemandRatio` y lo sirve el surplus de
fuentes con residual tras hard.

### 3.5 Convergencia (cota de epochs)

Estado: vector de `m_AllocatedPower` en edges SOURCE→C.

- Residual de un SOURCE de un outgoing es constante (`m_MaxOutput`).
- `ask_i = min(share, residual_i) + fill` es funcion **monotona no
  creciente** del pedido respecto a pedir de mas: nunca `ask_i > residual_i`,
  luego `totalHardDemand` del SOURCE hacia C es `<= available`, luego
  **no entra en overload por este merger**.
- All-off por otra rama (2.g) anula residual hacia C (0) y es estable
  hasta que esa rama cambia.
- Publicacion de demanda de C: 1 pass para `m_DemandKnown` / LastStable
  (`:2699-2703`, `:2707-2771`). Fuentes reencoladas same-epoch
  (`:2842-2846`).
- Cota: **2 ciclos de requeue** para 2.a (C publica 50, fuentes asignan
  20+30). Cold start: +1 (estima bootstrap → publish real). Recuperacion
  2.f: 1. Por debajo de `MAX_REQUEUE=5`.
- Oscilacion clasica (contar / no contar overload): **imposible** porque
  la fuente chica ya no overload por el split.

No hay barrido global nuevo: mismos bucles O(salidas) + O(incoming K≤2).

---

## 4. Invariantes

**I1 Conservacion de fuente.** Para cada SOURCE/PASSTHROUGH P:
`sum_e m_AllocatedPower(e) <= availableOutput(P) + LFPG_PROPAGATION_EPSILON`
en el epoch estable. Comprobable: suma de edges outgoing vs `allocAvail`.

**I2 Conservacion de PASSTHROUGH merger (Combiner/Splitter fusion).**
`|sum incoming alloc - sum outgoing alloc| <= epsilon + selfConsumption`
salvo deficit (outgoing = min(demanda, incoming)). Nunca outgoing > incoming
+ virtualGen. Combiner consumo 0.

**I3 Localidad.** Un cambio de alloc en el componente de C no modifica
`m_ComponentId` ni `m_OutputPower` / alloc de nodos cuyo
`m_ComponentId` es distinto. 2.c isla Z.

**I4 Regresion cero un proveedor.** Si `CountPoweredIncoming==1` (o 0),
el texto ejecutado es el de hoy: sin cap-and-fill, mismo cold-start v2.4,
mismo all-off. La mayoria de redes (Splitter 1 IN, `LFPG_Splitter.c:53-54`).

**I5 No oscilacion.** En topologia fija y demandas fijas, a partir del
epoch T0+2 el vector de alloc SOURCE→C no cambia mas de epsilon.
Requeues por nodo ≤ 5.

**I6 Soft no sobrecarga.** `totalHardDemand` excluye soft (`:4049-4060`)
se mantiene.

**I7 All-off SOURCE intacto.** Si el hard **pedido** (ya capado) a un
SOURCE supera su available (p. ej. 2.g + otra carga), todas sus salidas
siguen a 0.

---

## 5. Superficie del cambio (ronda de implementacion)

**Si se toca (esperado):**

- `scripts/5_Mission/LFPG_ElecGraphImpl.c`
  - `AllocateOutput` Pass 1 divisor `:4023-4028` (reemplazar `/ ptPoweredIn`
    por cap-and-fill / `SplitTwoProviderHard`).
  - Posible helper junto a `CountPoweredIncoming` (`:3845-3901`).
  - Cold-start tope v2.4 `:4011-4014`: aplicar **post-split** (decision §3.3).
  - Overload all-off de **PASSTHROUGH merger** cuando hard > available:
    `:4058-4110` — excepcion documentada en 2.b (solo si fan-in>1 y
    consumo propio 0, o flag “combiner”). **No** cambiar SOURCE.
- Test nuevo: `.github/tools/test_graph_multifeed_split.py` (nombre
  tentativo; ronda 2).

**No se toca:**

- Persistencia `OnStoreSave/Load`, RPC, SyncVars, classnames
  `CfgVehicles`, `LFPG_ConnectionRules.c`.
- `LFPG_Combiner.c` / puertos / cap 500.
- Zona cargadores vanilla `TrackVanillaCharger`..`TickVanillaChargers`
  (lane i77).
- `MarkUpstreamNodesDirty` / CutAll / cortes de puerto (lane i78).
- `.github/tools/graph_reference/` (lane i72).

**Coste/epoch:** +O(K) con K=incoming del target (ya se paga
`CountPoweredIncoming`). Cero barridos de `m_Nodes`. Memos iguales.

---

## 6. Plan de pruebas

### 6.1 Offline (slice)

Patron: `.github/tools/enforce_scalar_slice.py` +
`.github/tools/test_graph_capacity_refresh.py`.

`scalar_function` **rechaza `for`/`while`** (`enforce_scalar_slice.py:26-27`).
`AllocateOutput` entero no es sliceable. Por eso el nucleo
`SplitTwoProviderHard` es scalar (solo if/assign/return).

Sentencias de produccion a cargar:

- El helper nuevo extraido de `LFPG_ElecGraphImpl.c` (mismo texto que
  corre el grafo).
- Opcional: el bloque de decision `totalHardDemand > available + eps`
  si se extrae a `IsHardOverload(available, totalHard)` (hoy `:4058-4061`).

Tests (positivos + negativos):

| id | entrada | pasa si | la BASE (HEAD) falla |
|---|---|---|---|
| a | d=50, rA=20, rB=50 | askA=20, askB=30 | equal split 25+25 |
| b | d=80, rA=20, rB=50 | 20+50, sum=70 | 40+40 |
| c | d=50, rA=20, rB=40 (S50 ya dio 10) | 20+30 | 25+25 |
| d | dHard=50, soft aparte | split hard 20+30; soft no entra al helper | n/a en helper |
| e | d bootstrap min(500,70)=70 → fill 20+50 luego epoch2 d=50 | no ask>residual | tope asimetrico |
| f | d: 50→0 | ask 0+0 | n/a |
| g | rB=0 (otra rama all-off), d=50 | 20+0 | 25+25 y S20 overload |

**Negativos:** mutar `askA = share` sin tope residual (como
`test_negative_control_missing_passthrough_assignment` en
`test_graph_capacity_refresh.py:72-77`) y comprobar que (a) deja de
cumplir 20.

**Gate de esta spec:** contra el codigo de HEAD, un oraculo Python del
split **igual** `d/2` (reimplementacion del `:4027`) produce 25+25 y
**falla** el assert 20+30. Constancia: se razono con el codigo leido;
esta ronda no ejecuta Python (brief: sin shell). La ronda 2 **debe**
correr el test contra `git show HEAD:scripts/5_Mission/LFPG_ElecGraphImpl.c`
(falla) y contra el arbol con helper (pasa).

### 6.2 Referente independiente (lane i72, no visible)

Interfaz minima que G-01 necesitara **despues**:

**Entrada (JSON o funciones Python):**

- `nodes[]`: `id`, `type` (`SOURCE|PASSTHROUGH|CONSUMER`), `capacity`
  (`m_MaxOutput`), `consumption`, `softDemand` (0 default),
  `virtualGeneration` (0), `componentId`.
- `edges[]`: `source`, `target`, `enabled` (bool).
- `hardDemands` implicitos en consumption de CONSUMER; PASSTHROUGH
  demand = f(downstream).

**Salida:**

- `allocation[edgeId] -> float` factible (I1, I2).
- `maxHardServable` (float): max hard que la red puede servir en el
  componente (para 2.b = 70).
- `overloadedSources[]` de ids.

G-01 compara alloc estable del grafo (tras N epochs simulados o el
helper) con el oraculo en tolerancia `LFPG_PROPAGATION_EPSILON`. No se
implementa el oraculo en esta lane.

### 6.3 Protocolo in-game (PENDIENTE-INGAME)

No hay fuente 20+50 exactas aparte de solares:

- `LFPG_SolarPanel` → 20 u/s (`LFPG_SolarPanel.c:89`).
- `LFPG_SolarPanel_T2` → 50 u/s (`:230-232`).
- Alternativa 50: `LFPG_DEFAULT_SOURCE_CAPACITY` (`Defines.c:183`) /
  horno `LFPG_FURNACE_CAPACITY=50` (`Defines.c:731`) de dia/noche.
- Combiner: `LFPG_Combiner`.
- Hard 50: `LFPG_WaterPump` (`LFPG_PUMP_CONSUMPTION=50`, `Defines.c:709`).
- Hard 80: bomba 50 + `LFPG_Fridge` 20 (`LFPG_Fridge.c:15`) +
  `LFPG_LampDeviceBase` 10 (`:38`) en paralelo via Splitter a la salida
  del Combiner.
- Soft: `LFPG_Battery_*` en carga (`m_SoftDemand` / ratio).

Montaje (a): Panel 20 y T2 50 → cables a `input_1`/`input_2` del Combiner
→ bomba. De dia, fuentes ON.

Lecturas: inspector `m_LoadRatio` / `m_Overloaded` (SyncVars del panel
`LFPG_SolarPanel.c:76-81` y Combiner `:60-63`); color de cable; bomba
`LFPG_IsPowered`. Esperado: paneles sin overload, bomba ON, load S20≈1.0,
S50≈0.6.

(b) anadir nevera+lampara: bomba puede quedar corta (70<80); ni panel
en CRITICAL all-off.

Localidad: segunda isla panel T2 + bomba, distinta base; tras (a) no
cambia.

Accion recuperacion: cortar cable de la bomba; ambos paneles IDLE en
≤ ~300 ms (3 ticks de 100 ms, `LFPG_PROPAGATE_TICK_MS`, `Defines.c:508`).

---

## HALLAZGOS-ADYACENTES

- `CountPoweredIncoming` usa `m_OutputPower` del SOURCE, no la alloc
  real: una fuente en overload sigue “powered”. No se “arregla”
  aislado: el cap-and-fill lo hace irrelevante para el pedido.
- `GetEdgeAllocatedPower` fallback equal-split en SOURCE (`:4289-4298`)
  puede filtrar potencia en el primer pass; fuera de G-01.
- PASSTHROUGH all-off con deficit (2.b) es un segundo bug acoplado;
  esta spec lo incluye porque sin eso el criterio de deficit no se
  puede cumplir.

---

## LO QUE NO PUDE VERIFICAR

- Ejecucion de Python / linter / juego: brief de esta ronda sin shell.
  El fallo de (a) contra HEAD esta **razonado** con el divisor
  `:4027`, no corrido.
- Lineas exactas en `b7b917e` (issue dice divisor `:4027`, overload
  `:4058`): en el arbol actual coinciden en contenido; no hice
  `git show b7b917e`.
- Si algun PASSTHROUGH de produccion tiene fan-in > 2 (el Combiner
  tiene 2). `LFPG_MAX_EDGES_PER_NODE=12` (`Defines.c:522`) lo permite
  en el grafo.
- Comportamiento diurno real de `LFPG_SolarPanel` (si de noche
  `m_SourceOn` baja a 0): no leido el timer.
- Oraculo i72: no existe en este worktree.
- Que `m_AllocatedPower` de otras salidas en Pass 1 ya este actualizado
  al calcular residual (orden del array `m_Outgoing`): no verificado
  el orden de insercion.

---

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- El issue trata `20+30` como *la* asignacion factible; tambien lo es
  `14.29+35.71` (proporcional a residual). Si el referente i72 maximiza
  otra funcion (p. ej. proporcional estricto), el criterio de cierre
  “comparacion contra referente” puede **chocar** con `20+30`. Esta spec
  fija cap-and-fill igualitario y lo declara.
- “Si el cambio es local, `componentId` y potencia de islas ajenas no se
  tocan”: AllocateOutput ya es local al nodo; un SOURCE compartido (2.c)
  **no** es isla ajena. La premisa mezcla localidad de componente con
  fan-out de una fuente.
- “No basta con excluir overload ni ponderar nominal”: correcto para
  conservacion+recuperacion; una de esas dos *casi* resuelve (a) y
  oscila. El brief asume que hay que spec antes de PR: de acuerdo.
- Tope v2.4 (`:4011-4014`) asume un solo `availableOutput` (el del
  llamador). En multifuente esa premisa es falsa; hay que mover el tope
  post-split. Eso es un cambio del cold-start escrito para baterias, no
  pedido explicitamente por #76, pero (e) lo exige.
- All-off en Combiner con deficit (2.b) puede ser intencional
  (binario v1.0, comentario `:3903-3905`). Relajarlo solo en fan-in>1
  es la decision de esta spec; si el dueno quiere all-off universal,
  2.b **no** tiene asignacion factible no-cero y hay que reescribir el
  criterio.
