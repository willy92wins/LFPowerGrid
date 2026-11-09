<!-- Salida literal de Grok (solo lectura, sesion nueva, a ciegas). Quitado solo el preambulo previo al titulo. -->

# PLAN — grafo Splitter / puertas / Laser→Set (1.3.1)

Base: `main` `592ebcc`. Solo lectura. No implementa. AGENTS.md manda sobre el brief si chocan: aquí no chocan salvo que el brief pide volcar `PLAN-grok.md` y este encargo pide el plan por stdout sin escribir disco.

## Veredicto

Hay **dos causas distintas** en el mismo asignador. La hipótesis del brief acierta en la lectura por flujo y en D5; **falla** como explicación de A3 y **falla** al decir por qué el montaje sin Splitter pasa. La dirección candidata (sonda siempre en entradas de señal) **arregla D5 si sube la demanda aguas arriba del Laser**; **no arregla A3**.

## Hipótesis del brief, una a una

### H1. Las puertas leen entradas por flujo asignado — **SE SOSTIENE**

`LFPG_LogicGateBase.LFPG_SetPowered` llama `nm.IsPortReceivingPower` en `input_0` / `input_1` (`scripts/4_World/LFPG_LogicGate.c:141-142`). La Memory Cell igual en Power/Toggle/Reset/Set (`scripts/4_World/LFPG_MemoryCell.c:201-208`).

`IsPortReceivingPower` (`scripts/5_Mission/LFPG_ElecGraphImpl.c:5013-5048`) recorre aristas entrantes habilitadas del puerto y devuelve true solo si `edge.m_AllocatedPower > LFPG_PROPAGATION_EPSILON` (`:5044`). Epsilon real: `0.001` (`scripts/3_Game/LFPG_Defines.c:523`).

### H2. Demanda de un PASSTHROUGH = `m_LastStableOutput`; el merger puede dejar una entrada a 0 — **SE SOSTIENE para D5; NO es la causa de A3**

Demanda en `AllocateOutput` Pass 1: si el destino es PASSTHROUGH, `edgeDemand = targetNode.m_LastStableOutput` (`LFPG_ElecGraphImpl.c:4603-4605`). Luego `ApplyMergerWaterFill` (`:4390-4555`) si `MergerVectorCount > 1` (`:4411`).

`IncludeInMergerVector` (`:4009-4019`): el proveedor propio siempre entra; un hermano solo si `supplierPower > EPSILON`. `SupplierPowerForMerger` en PASSTHROUGH usa `PassthroughSupplyAt` (`:4056-4066`, `:4022-4034`): `input + virt - cons`, o `0` si `gateClosed`.

En D5 el Laser con `input=5` y `LFPG_LASER_CONSUMPTION=5.0` (`LFPG_Defines.c:406`) da oferta `5-5=0`. El hermano Power sí tiene oferta. El Laser **sí** entra en su propio `AllocateOutput` (`fromSelf=true`), con `ProviderOfferBase` 0, y reescribe `m_Demand` de Set a 0. Power, al no ver al Laser (`IncludeInMergerVector(false, 0)=false`), se queda con k=1 y carga toda la demanda de la celda. Set queda en `m_AllocatedPower=0`. Coherente con la medida (compuerta del laser abre, `set 0`).

En A3 XOR con A on / B off el merger **no** deja in0 a 0 por water-fill: B está cerrado → `PassthroughSupplyAt` 0 → k=1 sobre la puerta → no se parte la demanda. in0 pide los 10 u/s enteros de la lámpara.

### H3. La sonda `LFPG_GATE_PROBE_DEMAND` (1.0) solo con puerta cerrada; al abrir, oscilación — **PARCIAL**

Constante real: `LFPG_Defines.c:530`. Aplicación con puerta cerrada en PDQ: `LFPG_ElecGraphImpl.c:2658-2671` (`baseDemand = max(consumo, 1.0)`). En cold-start de `AllocateOutput`: `:4634-4649`.

Al abrir, esa rama no corre: la demanda pasa a `demandSignal` (aguas abajo + autoconsumo) (`:2549-2631`). Eso es cierto.

La oscilación de A3 **no** es «una entrada se queda sin su parte del reparto y la puerta se cierra». Es la **política binaria all-off** del Splitter cuando `totalHardDemand > availableOutput` (`:4735-4737`, Pass 2 pone `newAlloc=0` en todas las aristas `:4762-4787`). `availableOutput` del Splitter es el `inputSum` de **esta** época (`:2519-2525`, `:2374-2417`), que aún es la sonda previa (~2), no los 50 del generador.

### H4. Sin Splitter las entradas vienen de proveedores distintos y el reparto no deja ninguna a 0 — **CAE**

Sin Splitter el merger de la puerta **sigue existiendo**. AND/OR con ambas on sigue partiendo `m_LastStableOutput` (10) en 5+5. Lo que cambia es el `availableOutput` del nodo que alimenta los interruptores: es un **SOURCE** con `newOutput = m_MaxOutput` (`:2366-2372`), 50 u/s (`LFPG_DEFAULT_SOURCE_CAPACITY`, `LFPG_Defines.c:183`; en partida `gen[… o50]`). `11 ≤ 50` → no hay overload → cada arista recibe su demanda completa.

Eso explica el control medido (AND 0001, OR 0111, XOR 0110). Una causa que no distinga SOURCE.available=50 frente a PASSTHROUGH.available=inputSum_época no vale.

### H5. TA3-01 umbrales `IsPortReceivingPower` vs `m_AllocChanged` — **contexto, no causa**

`m_AllocChanged` se enciende si `|newAlloc - m_AllocatedPower| > EPSILON` (`:4788-4794`). Un cruce del booleano `> EPSILON` con delta numérico `< EPSILON` no reencola. No hace falta tocarlo para A3/D5. **No se toca.**

## Por qué el mismo montaje SIN Splitter sí funciona

Cadena A3 sin Splitter: `gen.o1 → swA → gate.in0`, `gen.o2 → swB → gate.in1`, `gate.out → lámpara 10`.

1. SOURCE en PDQ: `allocAvail = 50` siempre que esté encendido.
2. Interruptor cerrado: demanda sonda 1. Abierto: demanda = `m_LastStableOutput` de la puerta (10 con lámpara, 1 si la puerta sigue cerrada).
3. XOR A on B off, puerta abierta: demandas 10 + 1 = 11. `11 > 50 + 0.001` es falso. All-off no dispara. swA recibe 10, swB recibe 1 (sonda; su salida hacia la puerta es 0 porque `gateIsClosed` pone `allocAvail=0` en B, `:2520-2525`).
4. `IsPortReceivingPower(in0)=true`, `in1=false`. XOR se queda abierto. `m_LastStableOutput` de la puerta se queda en 10. No hay salto 2→11 contra un techo 2.

Con Splitter, el techo de la época en que la puerta abre es el flujo que el generador **ya** mandó (demanda estable previa ≈ 2), y 11>2 dispara all-off. El latch de brownout de la puerta (`LFPG_LogicGate.c:166-180`) aguanta **una** evaluación a oscuras; la segunda lectura (0,0) cierra. Vuelve la sonda, in0 vuelve a ser 1, XOR abre, y el ciclo de ~4 s de la medida se explica con eso (ticks de 100 ms, `LFPG_PROPAGATE_TICK_MS`, `LFPG_Defines.c:534`, más requeues y el hold).

## Circuito A3, paso a paso

Montaje: `gen (SOURCE, 50) → spl (PASSTHROUGH, cons=0, cap=200, no gated)`; `spl.o1 → swA`; `spl.o2 → swB`; `swA → xor.in0`; `swB → xor.in1`; `xor.out → lamp (CONSUMER, 10)`. Consumos: Splitter 0 (`LFPG_Splitter.c:69`), Switch 0 (`LFPG_SwitchDeviceBase.c:31`), puerta 0 (`LFPG_LogicGate.c:113`), lámpara 10 (`LFPG_LampDeviceBase.c:38`). Switches y XOR son gated (`LFPG_IsGateCapable` true).

Estado estable previo (XOR cerrado, A on B off, ambas sondas):

| Arista | demanda | alloc |
|---|---|---|
| gen→spl | lastStable spl ≈ 2 | 2 |
| spl→swA | 1 (puerta cerrada, lastStable XOR=1) | 1 |
| spl→swB | 1 (B cerrado, sonda) | 1 |
| swA→xor | 1 | 1 |
| swB→xor | 1 pero allocAvail B=0 | 0 |
| xor→lamp | 10 | 0 (XOR cerrado) |

`IsPortReceivingPower(in0)`: 1 > 0.001 → true. in1: 0 → false. XOR abre. `m_AllocChanged` en transición de puerta (`:2688-2690`) ensucia aguas arriba.

**Época en que XOR ya está abierto, Splitter aún con input=2**

1. **gen** `ProcessDirtyQueue`: `newOutput=50`. `AllocateOutput(gen, 50)`: demanda de spl = lastStable spl (aún ~2 o ya 11 si el XOR publicó antes; el orden BFS típico es gen→spl→sw→xor; el XOR publica 10 y reencola spl **en la misma época** acotado por `LFPG_MAX_REQUEUE_PER_EPOCH=5`, `LFPG_Defines.c:494`). Tras publicar XOR: lastStable XOR=10 (`demandSignal=10+0`).
2. **swA** abierto: `AllocateOutput(swA, inputA)`. Destino XOR es merger. B cerrado → k=1 → no reescribe. `edgeDemand=10`. Si inputA aún es 1: `10 > 1` → **swA overloaded**, arista swA→xor **0**. Si el Splitter aún no ha subido, esto ya apaga in0.
3. **spl** (el que manda): `inputSum=2`, `allocAvail=2`. Demandas: swA lastStable=10, swB=1, totalHard=11. `11 > 2+0.001` → `overloaded=true` → **spl→swA = 0 y spl→swB = 0**.
4. **XOR** `inputSum=0`. `AllocateOutput` con puerta abierta y available 0: lámpara 10 > 0 → all-off. `IsPortReceivingPower` in0=in1=false. Primer (0,0): hold. Segundo: `m_GateOpen=false`. lastStable vuelve a sonda 1 (`:2658-2671`).
5. Siguiente ciclo: demandas 1+1=2 ≤ 2, alloc 1+1, in0 true, XOR abre otra vez.

Números que casan con el log de partida (`GRAFO-SPLITTER-MEDICIONES.md`): `spl[p1 i2 o2]` → `spl[p1 i11 o2]` (demanda 11, input aún chico) → `spl[… o11]` con `a[p0 i0 o10]` (all-off, demanda residual 10) → gate in 0 0.

AND/OR con Splitter: misma palanca. Con ambas on, merger sí parte 10 en 5+5 (`WaterFillShareAsk`: `demand/k` capado por cap, `:4096-4116`); el Splitter ve total 10 contra input 2 y vuelve a all-off. Por eso AND queda 0000 y XOR 0000; OR 0001 en una muestra puede ser un fotograma en el que coincidió alloc no nulo.

## Circuito D5, paso a paso

Montaje: `gen.o1 → laser.in`; `gen.o2 → cell.input_0 (Power)`; `gen.o3 → pulsador → cell.input_2 (Reset)`; `laser.out → cell.input_3 (Set)`; `cell.output_0 → lamp 10`. Celda: PASSTHROUGH no gated, cons=1 (`LFPG_MemoryCell.c:21,143`), cap=100. Laser: gated, cons=5, cap propia.

Laser cerrado (nadie en el haz): PDQ `:2658-2671` → demanda `max(5, 1)=5`. Gen asigna 5. Laser powered, raycast corre, puede abrir. Salida 0 (`allocAvail=0`). Set=0. Correcto.

Laser **abre** (coche en el haz):

1. **cell** (aún latch 0, lámpara apagada): `demandSignal = 10` (si hay lastStable de lámpara en frío) **o** `1` (solo autoconsumo) + aguas abajo cuando el latch suba. En el escenario Nostra la lámpara está cableada: con latch 0, `SetOutputPortEnabled` deja output_0 deshabilitado; demanda de celda ≈ autoconsumo 1. Tras Set verdadero, lastStable ≈ 11 (10+1).
2. **Power (gen.o2)** `AllocateOutput`: destino cell, merger. Vector: Power (self) + Laser si `PassthroughSupplyAt(laser) > 0`. Laser: `input=5`, abierto, `5-5=0` → **no entra**. k=1. Power se queda con D completa (1 o 11).
3. **Laser** `AllocateOutput(laser, allocAvail=0` en el tick de apertura, luego `input-5=0`)`: `fromSelf` mete al Laser en el vector; `baseP = ComputeOfferBasePassthrough(..., incoming, 0, 5, false)` (`:4074-4092`) = `max(0, in-5)=0`. `remaining=0`. `selfAsk=0`. `m_Demand` Set ← 0. `m_AllocatedPower` Set ← 0.
4. `IsPortReceivingPower(cell, input_3)` false. `hasSet` false (`LFPG_MemoryCell.c:235-237`). Latch no sube. Medida: `gate 1 set 0 active 0`.

Variante interruptor en Set, Power aparte: Switch cons=0 → `PassthroughSupplyAt=input>0` → entra en merger → Set recibe share > ε → **pasa** (medida).

Variante Laser→interruptor→Set: el Laser sigue ofreciendo 0; el interruptor no tiene input; misma hambre. La nota del informe de medidas sobre esa variante **sí** queda explicada por la oferta 0, no hace falta otra raíz.

Sin cable en Power: k=1 solo Laser; no hay merger que lo excluya; D entera va al Laser; gen le da 5+D; leftover puede ser >0; Set se ve (medida `set 1 active 1`). Al salir del haz el Laser cierra y la lámpara se apaga porque Power no existe.

## Reproducción offline (ejecutable, rojo hoy)

Herramientas ya en el árbol:

- `.github/tools/enforce_scalar_slice.py`: extrae y ejecuta las funciones escalares reales (`IncludeInMergerVector`, `PassthroughSupplyAt`, `ComputeOfferBasePassthrough`, `WaterFillShareAsk`, …).
- `.github/tools/test_graph_multifeed_split.py`: `helpers()` carga esas slices; `QueueModel.allocate` **ya replica** Pass 1 + merger + all-off (`hard > available + EPS` → alloc 0, líneas 327-385).
- `.github/tools/graph_reference/`: oráculo max-flow **independiente**. No porta all-off ni el merger. Sirve para decir «el flujo físico existe» (A3 es factible a 10 u/s; D5 es factible a 11+5), no para el bug. Un `verify` contra allocs de producción en A3 marcará `hard_unmet` / `feasible_but_underfed` cuando el Splitter ha puesto 0.

**Hueco comprobado:** `QueueModel.process` para PASSTHROUGH **no** aplica el override de sonda con puerta cerrada (`ElecGraphImpl.c:2658-2671`). Sin ese ramo, el test no reproduce el salto 2→11. El test tiene que copiar ese ramo (subclase o 15 líneas en `process`) **antes** de montar A3. Eso no es el arreglo; es fidelidad del arnés.

### Test 1 — A3 XOR detrás de Splitter

Fichero: `.github/tools/test_graph_multifeed_split.py`, clase nueva p.ej. `TestGrafoSplitterLogic`.

Montaje `QueueModel`:

- `gen` SOURCE max 50
- `spl` PT max 200 cons 0
- `swA` PT gated open (`gate=False`) cons 0
- `swB` PT gated closed (`gate=True`) cons 0
- `xor` PT gated, empieza `gate=True`, cons 0, max 100
- `lamp` CONSUMER cons 10
- aristas: gen→spl, spl→swA, spl→swB, swA→xor tport `input_0`, swB→xor `input_1`, xor→lamp

Semilla `gen`. Varias `run()` (épocas). Tras cada época: si alloc(swA→xor)>0.001 and alloc(swB→xor)≤0.001 → abrir xor; si ambos ≤0.001 dos veces → cerrar xor (espejo del brownout).

**Aserción que hoy va rojo:** tras ≥3 épocas, `lamp` no alcanza alloc ≥ 10 de forma estable **o** `spl.m_Overloaded` es true en la época en que xor está abierto **o** `alloc(swA→xor)` no se queda > 0.001. Concreto recomendado:

```python
self.assertGreater(q.edges["e_swA_xor"].m_AllocatedPower, EPS)  # rojo: 0.0
self.assertGreaterEqual(q.edges["e_xor_lamp"].m_AllocatedPower, 10.0 - EPS)  # rojo: 0.0
self.assertFalse(q.nodes["spl"].m_Overloaded)  # rojo: True
```

Control negativo en el mismo fichero: los mismos nodos **sin** `spl`, swA/swB salen de dos salidas de `gen`. Las tres aserciones **verdes hoy**. Eso muerde la causa (available 50 vs input 2), no un fixture tautológico.

### Test 2 — D5 Laser → Set

Montaje: `gen` 50, tres salidas; `laser` PT cons 5 gate open; `cell` PT cons 1 no gated last_stable 11 known; `lamp` 10; Reset opcional.

**Aserción roja hoy:**

```python
self.assertGreater(q.edges["e_laser_set"].m_AllocatedPower, EPS)  # rojo: 0.0
# y slices:
self.assertEqual(h["pt_supply"](5.0, 0.0, 5.0, False), 0.0)  # verde ya
self.assertFalse(h["include"](False, 0.0))  # verde ya (test_offer_excludes_self_hard)
```

El slice `pt_supply`/`include` ya está verde y **no** basta: hay que montar el circuito para que el alloc de Set sea 0.

Con el arreglo: A3 control y A3 Splitter verdes; D5 `e_laser_set > EPS` y latch Set legible; fixtures actuales de `test_graph_multifeed_split.py` y `test_graph_reference.py` sin cambios de números salvo los justificados abajo.

Mutantes que muerden (R26):

1. Quitar el all-off solo en el test-double y no en Enforce → test Python verde, linter/partida roja. El arreglo tiene que vivir en `AllocateOutput`/`PDQ` real.
2. Forzar `IncludeInMergerVector(..., 0)=true` sin subir demanda del Laser → Set sigue a 0 (remaining 0). El mutante exige la subida de demanda aguas arriba.
3. Poner sonda en las dos entradas de la XOR **sin** cambiar all-off del Splitter → A3 sigue rojo (11 o 12 > 2).
4. Overload real de SOURCE: gen 50, dos lámparas 30+30 sin Splitter → sigue all-off (B5). Test de no-regresión.

Códigos de salida: `python -m unittest .github.tools.test_graph_multifeed_split` (o path que ya use CI) exit 1 hoy en los dos tests nuevos; 0 tras el arreglo. `test_graph_perf_probe.py` sin subir tiempo/nodos. Linter Enforce: gatear por `len(errors)==0`, no por `status` (AGENTS.md).

## Arreglo mínimo (R20/R25)

No implementar ahora. Trazado causa → línea.

### Parche 1 — A3: underfeed de PASSTHROUGH ≠ overload de SOURCE

Causa: `:4735-4737` compara demanda con `availableOutput` que en PT es input de esta época (`:2519`).

Cambio: en `AllocateOutput`, si el nodo es PASSTHROUGH, la decisión `overloaded` usa el techo de oferta (`ProviderOfferBase` / `ComputeOfferBasePassthrough` / suma de ofertas entrantes), no el input actual. Si `demand > input` pero `demand ≤ oferta aguas arriba` (gen 50, spl cap 200): **no** poner todas las aristas a 0; asignar `min(demanda_arista, remaining input)` en orden estable (el sort de merger ya existe) o prorrateo hard. SOURCE no cambia: `ComputeOfferBaseSource = availableOutput = 50` (`:4069-4071`) → B5 intacto.

Líneas: `AllocateOutput` `:4562-4796`; posiblemente PDQ `:2519-2525` solo si se decide pasar oferta en vez de input. No tocar `:2658-2671` (sonda cerrada).

Convergencia: época N alloc parcial (~2 hacia swA), XOR se queda abierto (in0>ε), lastStable 10, gen ve 11, época N+1 alloc 10+1, lámpara 10. Requeue misma época ya existe (`:2786-2836`, tope 5).

### Parche 2 — D5: trickle de salida en PASSTHROUGH gated con autoconsumo

Causa: `PassthroughSupplyAt` 5-5=0 (`:4022-4034`) + `IncludeInMergerVector` (`:4015`) + demanda abierta del Laser = solo consumo (`:2619-2621`).

Cambio mínimo alineado con la candidata **solo para D5**: con puerta **abierta**, `demandSignal = max(demandSignal, consumption + LFPG_GATE_PROBE_DEMAND)` (o `+ sonda` si consumption ya cuenta). Coste 1 u/s extra en el Laser abierto. Gen asigna 6; leftover 1; merger mete al Laser; Set recibe ≥1.

Alternativa evaluada y **descartada como única**: leer lógica por oferta (`m_OfferedResidual`) en `IsPortReceivingPower`. Cambiaría Splitter/Combiner/consumidores si se aplica al helper común; el brief pide que no-lógicos queden iguales. Si se hiciera, sería un predicado nuevo `IsPortReceivingLogic` usado solo por LogicGate/MemoryCell/Counter, no el helper actual.

Suelo de sonda en entradas de señal de la celda **después** del water-fill: con remaining Laser=0, `max(selfAsk, 1)` se recorta a 0 (`:4543-4546`). **No basta** sin el parche 2.

Puertas AND/OR/XOR: el parche 1 basta; no hace falta que in0/in1 pidan sonda extra abierta (subiría 10→12 y empeoraría el all-off si el parche 1 no va).

## Tests positivos / negativos

| Caso | Hoy | Tras arreglo |
|---|---|---|
| A3 XOR Splitter A on B off, lámpara 10 | rojo, spl overload | verde, lamp 10, 0–1 cambio de gate |
| A3 AND/OR tablas Splitter | rojo | verde |
| A3 sin Splitter | verde | verde (bit a bit alloc) |
| D5 Laser abierto + Power + Set | Set alloc 0 | Set alloc ≥1 |
| D5 interruptor en Set + Power | verde | verde |
| D5 sin Power, solo Laser | Set puede 1 (medida) | igual |
| B5 SOURCE sobrecargado (demanda > 50) | all-off | all-off |
| C1–C7 pulsadores Set/Reset/Toggle | verde (filtro 400 ms) | no tocar `LFPG_MCELL_DEBOUNCE_MS` |
| Fixtures `graph_reference/fixtures/*.json` | verify como ahora | mismos conjuntos de reglas |
| Combiner 20+50, baterías, shared_source | números actuales | sin delta |

## Riesgos del brief, uno a uno

**Asignación binaria B5 / sondas cerca del límite.** No se sube la demanda de SOURCE. Sonda extra solo en Laser (y análogos gated+cons) abiertos: +1 u/s. Un Splitter con tres Lasers abiertos pide +3; sigue << 50. B5 (distribuidor SOURCE que pone 0 si demanda>capacidad) **no se toca**. Test de no-regresión: dos consumidores 30+30 en un gen 50 → todas las salidas 0.

**Regresión no lógicos (Splitter, Combiner, baterías, consumidores).** Parche 1 solo cambia la condición de overload cuando el nodo es PASSTHROUGH **y** la oferta aguas arriba cubre. Splitter en topología de solo lámparas sin salto de puerta: input ya iguala demanda, rama nueva no dispara. Combiner/baterías: `totalSoftDemand` y Pass 3 (`:4801+`) igual. Consumidores: siguen `m_Consumption` (`:4599-4601`). Fixtures G-01 existentes: justificar uno a uno si algún número de underfeed transitorio cambia; el estable no.

**Convergencia / requeue misma época.** Parche 1 evita el ping-pong open/overload/close. Tope 5 requeues se mantiene. Prohibido reencolar sin `m_AllocChanged`/`outputDelta`/`inputChanged`. Test A3: en 80 pasos de `QueueModel.run` no oscilar `m_GateClosed`. `test_graph_perf_probe.py`: no empeorar.

**Coste por tick.** Sin aristas nuevas. Un `ProviderOfferBase` extra por `AllocateOutput` de PT (ya se llama dentro de merger `:4439`). Orden igual.

**Persistencia y sync.** No se toca `OnStoreSave`/`OnStoreLoad`, classnames, RPC, ni el formato de `m_LatchState`. `m_BrownoutHeld` sigue sin net-sync (`LFPG_LogicGate.c:80-84`). Nada de lo que se guarda cambia.

**TA3-01.** No se toca el umbral de `m_AllocChanged`. Si el parche 2 solo mueve demandas ≥1, no se cruza el hueco 0.0009↔0.0011.

## Qué no se toca

- Política all-off de **SOURCE** (B5).
- `IsPortReceivingPower` (salvo que D5 se hiciera por oferta; este plan no lo hace).
- Filtro 400 ms de Memory Cell; C1–C7.
- `LFPG_GATE_PROBE_DEMAND` para puertas **cerradas**.
- Combiner water-fill, baterías soft, `m_SoftDemandRatio`.
- `config.cpp` / classnames / persistencia.
- TA3-01.
- Lanzar DayZ (AGENTS.md §6): partida es fase 3.

## Comprobado vs supuesto

**Comprobado (leído `path:line` en este árbol):** H1, firmas de merger/sonda/all-off/PDQ available, consumos, `QueueModel.allocate` all-off, ausencia del override de sonda en `QueueModel.process`, medidas de partida en `GRAFO-SPLITTER-MEDICIONES.md`.

**Supuesto 1:** el orden BFS en la época del salto procesa Splitter con `inputSum` aún igual a la sonda. Prueba que lo tumba: un log `[PT-CHAIN]` o un `QueueModel` instrumentado que muestre `inputSum≥11` **antes** del `AllocateOutput` del Splitter en esa misma época; entonces A3 no overload y la causa sería otra (p.ej. merger 5+5 con un cap 0).

**Supuesto 2:** XOR en partida oscila por el segundo (0,0) del brownout, no porque `EvaluateGateLogic` reciba (1,1). Prueba que lo tumba: `logic_state` con in0=1 in1=1 en el tramo A4; entonces el merger sí estaría metiendo flujo en in1 con B “off”.

**Supuesto 3:** D5 con lámpara cableada tiene lastStable celda ≈11 cuando el latch está a 0 (output_0 disabled). Si lastStable es solo 1, el Laser sigue ofreciendo 0 y Set sigue a 0; el parche 2 sigue siendo necesario. Prueba: `m_LastStableOutput` de la celda en el tick del haz.

**Supuesto 4:** PressurePad y demás gated+cons caen como el Laser. No medido (el propio informe lo dice). El parche 2 los cubre por tipo, no por classname.

## Criterios de aceptación (fase posterior)

Offline: los dos tests nuevos rojo en `592ebcc`, verde con el parche; `test_graph_*.py` + oráculo verdes; fixtures sin delta injustificado; perf probe no peor. Partida: A1–A4 y B1–B2 con Splitter; C1–C7 y D5; regresión sin Splitter; ≤1 cambio en A4/60 s; cero `SCRIPT (E)` nuevos.

## AGENTS.md

Linter Enforce por `errors`, no por `status`. Sin ternarios/`++`/`foreach`/`ref` local. CRLF. Sin commit. Sin arrancar el juego en la implementación hasta fase 3.
