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

Suma de ofertas = 70 < 80. Politica de overload **vigente** (all-off,
`AllocateOutput:3903-3905` y `:4058-4110`): no se cambia en G-01.

**Hoy:** split 40+40. S20 overload (0). S50 alloc 40. C ve 40. Si C publico
80, `totalHardDemand=80 > available` → all-off en C. Consumidor 0. S20
tambien a 0.

**Exigido (G-01):**

- S20 entrega 20. S50 entrega 50. Ninguna fuente en overload (el pedido
  a cada una es `<=` su oferta).
- El Combiner recibe 70, `70 < 80` → overload **all-off**. Salida 0.
- Consumidor apagado.

El oraculo i72 marca esto `hard_unmet` y, si `hard_feasible` es false
(`max_hard_servable=70 < 80`), **no** `feasible_but_underfed`. G-01 no
persigue servir 70 aguas abajo del Combiner. El shedding (entregar 70
con C no all-off) esta fuera de alcance: ver
`## Propuesta para el dueno (fuera de G-01)`.

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

Oferta de Sp hacia C (B3): `min(200, 50) - 10 = 40`.
Oferta S20 = 20. Demanda C = 50. Water-fill: share 25 → 20+25, leftover 5
→ **20+30** hacia C. Sp pide 10 (l1) + 30 (C) = 40 a S50 ≤ 50.

**Hoy (trinquete):** C pide 25/25. S20 da 20 (o 0 si overload). Sp recibe
~25+20 y, si el residual hacia C se toma de `allocAvail`, se clava en 25
y nunca llega a 30. Ver traza §3.2.1.

**Exigido estable:** l1=10 ON, l2 (bomba)=50 ON, S20=20, Sp→C=30, S50=40
sin overload. Isla Z intacta. Cota de epochs: §3.2.1 (≤ 3 desde cold start,
≤ 2 desde el estado clavado 25/25).

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

**Exigido (M2):** water-fill de la misma `edgeDemand` que hoy parte el
divisor (LastStable hard+soft). D=70 → asks 20+50. Hard
`14.3+35.7=50`. Soft `5.7+14.3=20`. Bomba ON, batería 20. Pass 2/3
iguales (`:4098`, `:4035`).

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

**Exigido:** oferta S50→C = max(0, 50-60) = 0. S20 pide 20, no overload.
C recibe 20 con demanda 50 → overload all-off; bomba apagada. Si S50
pide 60 en la otra rama, all-off de S50 apaga también esa rama.

Si el consumidor 60 se apaga: residual S50→C vuelve a 50 y se reentra en
(a) exigido 20+30 en ≤ 2 epochs.

---

## 3. Algoritmo propuesto

### 3.1 Donde vive

Un edge E (P → C) es de merger si C es PASSTHROUGH y el **vector B4-1**
de E tiene 2 o más entradas (`MergerVectorCount > 1`). B1.3 sigue
usando `CountPoweredIncoming > 1`. Si ningún PASSTHROUGH tiene 2+
incoming enabled en ese vector, I4.

Fan-in alcanzable **no** esta acotado a 2. No se demuestra el contrario:

- Un puerto: `LFPG_MAX_FANOUT_PER_PORT = 1` (`Defines.c:521`). Ocupado
  no bloquea (`LFPG_ConnectionRules.c:61-64`); el servidor **reemplaza**
  el cable del mismo puerto (`FinishWiringCollectOwner` marca conflicto
  si `wire.m_TargetDeviceId == dstId && IncomingPortIndexKey(...) ==
  dstPort`, `LFPG_RPCServerHandlerImpl.c:722-724`). Un IN concreto
  queda con 1 wire.
- Varios IN en el mismo PASSTHROUGH: Combiner 2
  (`LFPG_Combiner.c:53-56`), LogicGate 2 (`LFPG_LogicGate.c:103-104`),
  ElectronicCounter 2 (`LFPG_ElectronicCounter.c:77-78`), Intercom 2
  (`LFPG_Intercom.c:125-127`), **MemoryCell 4** (`LFPG_MemoryCell.c:60-75`).
  Cada IN distinto admite un proveedor. `AddEdgeInternal` permite hasta
  `LFPG_MAX_EDGES_PER_NODE = 12` incoming (`ElecGraphImpl.c:1476-1484`,
  `Defines.c:522`).
- Vanilla: un IN tipico; el recuento powered puede ser 1. G-01 no asume
  que vanilla sea merger.

Por eso el split es **N-way** (§3.3), K ≤ 12, orden determinista.

### 3.2 Oferta residual (ultima escrita)

Un campo runtime en `LFPG_ElecEdge`: `m_OfferedResidual` (`LFPG_Data.c`
constructor `-1`). El grafo no se persiste; no hay SyncVar ni RPC de
edges. `-1` = nunca escrita. Las escrituras se recortan a `>= 0`. Un
`0` publicado es 0 (B2; mismo criterio F1 de demanda).

**Lectura.** Cada llamada lee la **ultima oferta escrita** del hermano
(`OfferCapFromWritten`). Solo el centinela `-1` usa fallback
`m_MaxOutput`. El cap **propio** es fresco (`ProviderOfferBase` de esta
llamada) y el ask propio nunca lo supera.

**Escritura** al cerrar `AllocateOutput` (`PublishEdgeOffers`):

```
offer(P->E) = max(0, base(P) - hard de las OTRAS salidas enabled de P)
base(SOURCE) = availableOutput
base(PASSTHROUGH) = 0 si gate cerrado; si no,
  min(m_MaxOutput, suma ofertas incoming + virt - consumo)
```

Formula unica B3. `ComputeOfferTowardEdge(baseP, totalHard, ownHard)`
= `max(0, baseP - (totalHard - ownHard))`.

**Vector B4-1.** Para E (P→C): entra E (cap = `remaining`) y cada
incoming enabled X de C tal que `IncludeInMergerVector(X sale de P,
SupplierPowerForMerger(origen X))`. `selfIndex` es `cpEdge == mEdge`.
Un PT sin entrada sigue en el vector y no pide D entero.

**B1.3 dirty.** Si la oferta de un edge cambia mas que epsilon o pasa
de nunca escrita a escrita (`ShouldNotifyOfferDirty`):
- si el target es merger (`CountPoweredIncoming > 1`), dirty de los
  otros proveedores;
- si el target es PASSTHROUGH que usa esa oferta como `base` y tiene
  una salida enabled a un nodo con 2+ incoming, dirty de ese PT.

Acotado por `LFPG_MAX_REQUEUE_PER_EPOCH`. Sin merger, no reencola (I4).

**Limitacion (cadenas largas).** S → SpA → SpB → C, con S20 directo a
C: SpA sale hacia SpB, que tiene **un** incoming. B1.3 no sucia SpB.
SpB puede quedar con `base` viejo. Consecuencia: C puede quedarse
corto hasta un dirty topológico. Fuera de cierre G-01.

**S1.3 punto fijo.** Cuando la cola está vacía, nadie tiene dirty
pendiente: todos leyeron las mismas últimas ofertas. Water-fill puro
⇒ `sum ask = min(D, Σcap)`.

**S1.4 transitorios.** El presupuesto parte la cola (`:2232-2235`);
requeues same-epoch (`:2097-2101`, `:2797-2800`, `:2842-2846`); cap
propio fresco vs oferta vieja de un hermano. Exceso en el merger ≤
una oferta stale (el hermano aún no recortó). Defecto ≤ lo que ese
hermano aún no ofrece. Se cierra cuando B1.3 (o `inputChanged`)
reprocesa al hermano; si el tope de requeue aplaza, el epoch
siguiente (`m_DeferredRequeue`).

**M1.** El cap fresco de P se reparte entre salidas a mergers en
orden (id target, puerto). Cada ask se recorta a `remaining`.
Limitación: no siempre es el max-flow global (`feasible_but_underfed`);
P nunca pide más que `base(P)`.

Coste: O(salidas) al publicar (un pase de hard + un pase de ofertas).
Los bucles nuevos suman `m_EdgesVisitedThisEpoch`.

#### 3.2.0 Orden de `m_Outgoing` (S3)

Pass 1 anota D (consumo / LastStable / cold-start **con tope v2.4 sobre
esa D**, no sobre `ask[self]`). `remaining` = base − hard de salidas
que no son merger. Water-fill pide `ask[self] ≤ remaining`. Dos
órdenes [l1,C] y [C,l1] dan el mismo remaining y los mismos asks.

#### 3.2.1 Traza 2.c

S50→Sp; Sp→l1(10) y Sp→C; S20→C; C→l2(50). Ofertas -1; fallback
`m_MaxOutput`. D de Pass 1 de cada proveedor (cold: tope a su
`availableOutput` si LastStable=0).

Sp pide más de lo que recibe: overload hasta que S50 reasigna en el
mismo epoch (`:2821-2851`).

| pase | caps leídas | D | ask S20 / Sp→C | l1 | l2 |
|---|---|---|---|---|---|
| cold | 20 y min(200,50)-10=40 | Pass 1 de cada uno | 20 / 30 al converger | ON | ON |
| estable | 20 y 40 | LastStable 50 | 20+30 | ON | ON |

Cota ≤ 3 epochs / requeues ≤ 5.

### 3.3 Water-fill N-way (mismo resultado en cada llamador)

Entrada: D = LastStable **entera** (hard+soft, M2). Vector = incoming
enabled del merger en el orden de `m_Incoming` (B4-1), el mismo array
para todos los proveedores. No se ordena por id.

Cota de bucle: `K = incoming.Count()` ≤ `LFPG_MAX_EDGES_PER_NODE` (12).
Dos pasadas: share y leftover. Total ≤ 24 iteraciones.

Pseudocodigo (estilo Enforce, sin ternarios ni `+=`; el slice de tests
puede especializar K=2 sin `for` — ver §6.1):

```
protected void WaterFillAsks(float D, array<float> cap, array<float> ask)
{
	int k = cap.Count();
	float eps = LFPG_PROPAGATION_EPSILON;
	if (k <= 0)
		return;
	float kf = k;
	float share = D / kf;
	float leftover = D;
	int i;
	for (i = 0; i < k; i = i + 1)
	{
		float a = share;
		if (a > cap[i])
			a = cap[i];
		if (a < 0.0)
			a = 0.0;
		ask[i] = a;
		leftover = leftover - a;
	}
	if (leftover < 0.0)
		leftover = 0.0;
	for (i = 0; i < k; i = i + 1)
	{
		if (leftover <= eps)
			break;
		float room = cap[i] - ask[i];
		if (room <= eps)
			continue;
		float add = leftover;
		if (add > room)
			add = room;
		ask[i] = ask[i] + add;
		leftover = leftover - add;
	}
}
```

K=2, cap 20 y 50, D=50: share 25 → 20+25, leftover 5 → **20+30**.
K=2, cap 20 y 50, D=80: 20+50, leftover 10 sin room → 20+50.

El proveedor i pone `edge.m_Demand = ask[i]`.

### 3.4 Prioridad hard sobre soft

Sin cambio de Pass 2/3. Water-fill reparte la D de Pass 1 (LastStable
entera). Pass 2 aplica `m_Demand * (1-ratio)`; Pass 3 el surplus.

### 3.5 Convergencia

- `ask[i] <= remaining` ⇒ el merger no mete a su proveedor en overload.
- Ofertas PT crecen cuando baja el hard de otras salidas; no se atan a
  `allocAvail`.
- Cota 2.a: 2 epochs. 2.c: ≤3. 2.f: 1. Requeue ≤ 5.
- Coste: O(salidas) + O(K) incoming del merger. Sin barrido de `m_Nodes`.

---

## 4. Invariantes

**I1 Conservacion de fuente.** Para cada SOURCE/PASSTHROUGH P:
`sum_e m_AllocatedPower(e) <= availableOutput(P) + LFPG_PROPAGATION_EPSILON`
en el epoch estable. Comprobable: suma de edges outgoing vs `allocAvail`.

**I2 Conservacion de PASSTHROUGH merger.** Solo se exige cuando el merger
**no** esta en overload. Entonces
`|sum incoming alloc - sum outgoing alloc| <= epsilon + selfConsumption`
y outgoing ≤ incoming + virtualGen. Con overload vigente, outgoing = 0
y incoming puede ser > 0 (2.b: entra 70, sale 0). Eso no viola I2
porque I2 no aplica en overload.

**I3 Localidad.** Un cambio de alloc en el componente de C no modifica
`m_ComponentId` ni `m_OutputPower` / alloc de nodos cuyo
`m_ComponentId` es distinto. 2.c isla Z.

**I4 Red sin merger.** Si ningún PASSTHROUGH tiene 2 o más incoming
enabled (vector B4-1 de tamaño < 2), `ApplyMergerWaterFill` no
reescribe demandas. Pass 2/3, overload y retorno son los de la base.

**I5 No oscilacion.** En topologia fija y demandas fijas, a partir del
epoch T0+3 (2.c; 2.a en T0+2) el vector de alloc no cambia mas de
epsilon. Requeues por nodo ≤ 5.

**I6 Soft no sobrecarga.** `totalHardDemand` excluye soft (`:4049-4060`)
se mantiene.

**I7 All-off SOURCE intacto.** Si el hard **pedido** (ya capado) a un
SOURCE supera su available (p. ej. 2.g + otra carga), todas sus salidas
siguen a 0.

---

## 5. Superficie del cambio (ronda de implementacion)

**Si se toca (esperado):**

- `scripts/3_Game/LFPG_Data.c` — `m_OfferedResidual` en `LFPG_ElecEdge`
  (centinela -1).
- `scripts/5_Mission/LFPG_ElecGraphImpl.c`: `OfferCapFromWritten`,
  `ComputeOfferTowardEdge`, `IncludeInMergerVector`,
  `SupplierPowerForMerger`, `WaterFillShareAsk`,
  `WaterFillLeftoverAdd`, `ApplyMergerWaterFill`, `PublishEdgeOffers`,
  `NotifyOfferChanged`. Tope v2.4 sigue en Pass 1 sobre D.
- **No** se toca overload `:4058-4110`.
- Test: `.github/tools/test_graph_multifeed_split.py`.

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
`.github/tools/test_graph_multifeed_split.py`.

`scalar_function` **rechaza `for`/`while`** (`enforce_scalar_slice.py:26-27`).
`AllocateOutput` entero no es sliceable. Los cuerpos
(`OfferCapFromWritten`, `ComputeOfferTowardEdge`, `WaterFillShareAsk`,
`WaterFillLeftoverAdd`, `IncludeInMergerVector`) se cargan con `load`.
N-way y cola van en el mismo test Python.

Criterio de cierre del issue: **factibilidad y conservacion contra el
oraculo**, no solo el par 20+30. 20+30 es regresion local de 2.a;
`Oracle.verify` es el de cierre.

| id | entrada | pasa si | BASE (HEAD) |
|---|---|---|---|
| a | D=50, cap 20/50 | 20+30; `hard_feasible`; verify ok | 25+25; oraculo `hard_unmet`+`feasible_but_underfed` |
| b | D=80, cap 20/50 | asks 20+50; fuentes no overload; C all-off; `max_hard_servable=70`; `hard_feasible` false; consumidor 0 (no parcial) | 40+40, S20 overload |
| c | cap 20/40, D=50 | 20+30 | 25+25 |
| c-traza | tabla §3.2.1 | l1 ON, l2 ON, 20/30/40 en ≤3 epochs; oferta Sp no es allocAvail | trinquete 25 |
| d | hard 50 + soft (bateria ratio 1) | water-fill LastStable; bomba ON 20+30; bateria 20 Pass 3 | split hard 25+25, bomba OFF |
| e | cold start tope v2.4 sobre D de Pass 1 | ask≤cap; epoch2 → 20+30 | tope asimetrico |
| f | D 50→0 | 0+0 | n/a |
| g | capB=0, D=50 | 20+0 | 25+25 |
| S3 | outgoing [l1,C] vs [C,l1] | committed y asks iguales | n/a |
| B4-1 | S50 off/on, 3/1/paralelo | l1 ON, S50 sin overload, bomba ON si cabe | pide D entero; clava |

**Negativos:** quitar el tope `if (a > cap)` (patron
`test_graph_capacity_refresh.py:72-77`); `ComputeOfferTowardEdge` sin
restar ownHard; `OfferCapFromWritten` con `return fallbackMax`;
`IncludeInMergerVector` sin `fromSelfNode`.

Gate HEAD: `D/2` falla (a) y `verify`.

### 6.2 Referente independiente (API i72)

Leido `C:\tmp\lfpg-grok\wt-i72\.github\tools\graph_reference\README.md`
mas `oracle.py` y `model.py` como interfaz. El README en disco lista
`hard_feasible`, `max_hard_servable`, `verify(g, allocation)` y reglas
`source_limit`, `edge_limit`, `edge_disabled`, `passthrough_limit`,
`conservation`, `hard_unmet`, `hard_priority`,
`feasible_but_underfed`, `non_negative`. El revisor anuncia ampliacion;
G-01 se ata a eso **y** a lo siguiente (el dueno integra el paquete
antes de implementar):

**Entrada** (`model.Node` / `Edge`):

- `SOURCE.available`; `PASSTHROUGH.pass_limit`, `self_consumption`,
  `virtual_generation`, `gate_closed` (cerrado ⇒ `pass_limit=0` en
  `model.py:38-39`: sin flujo aguas abajo; consumidores tras gate
  cerrado inalcanzables, fixture `gates_hard_soft.json`).
- `CONSUMER`/`CAMERA.hard_demand`. Consumidores **binarios**: o el hard
  entero o no servidos; `max_hard_servable` encaja con enteros de las
  fixtures (`combiner_20_50_hard50.json` pone 50).
- Soft: `soft_demand` (bateria llena = 0), `soft_fraction`.
- Edge: `id`, `src`, `dst`, `enabled`, `capacity`.

**Salida:** `hard_feasible`, `max_hard_servable` (2.a=50, 2.b=70),
`verify`. Conservacion a dos lados (`oracle.py:167-198`): SOURCE sin
inflow y outflow ≤ available; PASSTHROUGH outflow ≤ inflow+virtual;
CONSUMER sin outflow.

**Reglas extra anunciadas:**

- `over_allocation`: flujo > min(cap edge, oferta origen, demanda
  destino). El water-fill no la dispara.
- `partial_allocation`: binario con `0 < received < hard_demand`.
  2.a estable = 50. 2.b all-off = 0 (no parcial). 25 de 50 seria fallo.

En 2.b el oraculo puede marcar `hard_unmet` con `hard_feasible=false`.
El test de G-01 valida fuentes 20+50 (`source_limit` ok) y consumidor
no parcial. No se implementa el oraculo en esta lane.

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

(b) nevera+lampara (hard 80): paneles **sin** overload (20+50);
Combiner CRITICAL all-off; bomba apagada. No se espera servir 70.

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
- PASSTHROUGH all-off con deficit (2.b) tira 70 u/s ya asignadas. Fuera
  de G-01; ver propuesta al dueno.
- Si Σcap queda entre hard y D, el hard de C depende del surplus de
  Pass 3, que compite con otras salidas soft. Limitación del ratio
  vigente (`:4035`, `:4098`), no G-01.

---

## LO QUE NO PUDE VERIFICAR

- Ejecucion de Python / linter / juego: brief de esta ronda sin shell.
  El fallo de (a) contra HEAD esta **razonado** con el divisor
  `:4027`, no corrido.
- Lineas exactas en `b7b917e` (issue dice divisor `:4027`, overload
  `:4058`): en el arbol actual coinciden en contenido; no hice
  `git show b7b917e`.
- Comportamiento diurno real de `LFPG_SolarPanel` (timer de `m_SourceOn`).
- Paquete i72 integrado en **esta** rama (leido en `wt-i72`; reglas
  `over_allocation` / `partial_allocation` anunciadas, no estaban en el
  `oracle.py` leido: `grep` 0 hits ahi).
- Que el orden lexicografico de `m_DeviceId` sea estable entre sesiones
  (los ids son strings de dispositivo).

---

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- M1: con S40 en C1 y S20 en C2, el remaining 25/25 deja C2 en 45 y el
  oraculo marca `feasible_but_underfed` aunque 40+10 / 20+30 servirian
  ambas bombas. Seguridad (P ≤ 50) si; optimalidad global no. Fuera
  del criterio de cierre.
- El issue trata `20+30` como *la* asignacion factible; tambien lo es
  `14.29+35.71` (proporcional a residual). Esta spec fija water-fill
  igualitario.
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
- All-off en Combiner con deficit (2.b) es la politica v1.0
  (`:3903-3905`). G-01 la deja. El oraculo de max-flow **si** serviria
  70; verify de un alloc all-off no sale `ok`. El cierre “contra
  referente” en 2.b se interpreta como: fuentes factibles + no
  `feasible_but_underfed` (porque 80 no es factible). Si i72 exige
  outflow=70 en el Combiner, choca con S4.

---

## Propuesta para el dueno (fuera de G-01)

**Shedding en merger con deficit.** Hoy, Combiner con incoming 70 y
hard 80 pone todas las salidas a 0 (`:4058-4110`). Tirar 70 u/s ya
pagadas por las fuentes. Cambio posible, **no G-01**:

- Si PASSTHROUGH, fan-in>1, consumo propio 0: asignar
  `min(hard, availableOutput)` aguas abajo en vez de all-off.
- `m_Overloaded=true` para telemetria.
- I2 pasaria a exigir outgoing = incoming en deficit.

Hasta que el dueno lo acepte, 2.b queda: fuentes 20+50, C all-off,
consumidor apagado.

---

## Ronda 2: cambios

- **S1** — §3.2: snapshot `m_OfferedResidual` / `Prev` / `m_OfferEpoch`
  en `LFPG_ElecEdge`; lectura del epoch previo; prueba de suma
  `min(D,sum cap)`; exceso intra-epoch 0 si se respeta el snapshot.
- **S2** — oferta PASSTHROUGH por upstream+pass_limit, no `allocAvail`;
  2.c usa 40; traza §3.2.1 (20/30/10) cota 2–3 epochs.
- **S3** — §3.2.0 dos fases no-merger / merger; test S3 dos ordenes.
- **S4** — 2.b exigido all-off en C; I2 solo sin overload; shedding en
  propuesta al dueno; §5 sin excepcion de overload.
- **S5** — no se demuestra fan-in≤2 (MemoryCell 4 IN, tope 12);
  water-fill N-way §3.3, K≤12, orden por id.
- **S6** — tabla §6.1: 2.b segun S4, traza S2, S3, cierre via oraculo.
- **S7** — §6.2 contra README/oracle.py de i72 mas gates,
  consumidores binarios, conservacion dos lados, `over_allocation` y
  `partial_allocation`.

---

## Ronda 3: cambios

- **B1** — §3.2 lee la ultima oferta escrita; B1.3 dirty; S1.3 punto
  fijo / S1.4 transitorios. Sin `Prev` ni `m_OfferEpoch`.
- **B2** — centinela `-1`; un 0 escrito es 0.
- **B3** — formula unica `base - otherHard`; el propio edge no resta.
- **M1** — remaining entre mergers; limitacion max-flow en
  `QUE PUEDE ESTAR MAL` y fuera del cierre.
- **M2** — water-fill de LastStable completo; 2.d cifras 20+50 / 50+20.
- **m1** — I4: valores de red sin merger, no “texto identico de Pass 1”.
- **m2** — 2.g: C all-off, bomba apagada.
- **m3** — traza 2.c: Sp puede overload hasta el requeue upstream
  `:2821-2851` en el mismo epoch.
- **m4** — vector = mismos incoming que `CountPoweredIncoming`.

---

## Ronda 4: cambios

- **B4-1** — vector de merger: E siempre entra; otros incoming enabled
  si salen de P o si el origen pasa el predicado de
  `CountPoweredIncoming`. `selfIndex` por identidad de edge.
  `IncludeInMergerVector(fromSelfNode, supplierPower)`. Deteccion
  `MergerVectorCount > 1`. B1.3 sigue con `CountPoweredIncoming > 1`.
- **M4-1** — `MergerSortLess` compara `ToAscii` de cada caracter, no
  `string < string`.
- **M4-2** — `PublishEdgeOffers` suma hard una vez y
  `ComputeOfferTowardEdge(baseP, totalHard, ownHard)`.
  `ApplyMergerWaterFill` devuelve bool; totales solo si reescribio.
  Bucles nuevos incrementan `m_EdgesVisitedThisEpoch`.
  Borrados `OtherEnabledHard` y `SkipOtherIndex`.
- **M4-3** — §3 una sola regla (codigo). Sin snapshot Prev/Pub, sin
  oferta SOURCE/PT aparte, D = LastStable, orden `m_Incoming`, tope
  v2.4 sobre D de Pass 1.
- **m4-1** — recuperacion S50 en tres montajes; negativo sin
  `fromSelfNode`; S3 dos ordenes; 2.d bateria+bomba; negativos de
  comportamiento en `ComputeOfferTowardEdge` y last-write.
