<!-- Salida literal de Codex (codex exec, solo lectura, a ciegas). -->

# PLAN-codex — señales lógicas detrás de Splitter

**Propongo separar la señal lógica de la potencia asignada mediante un bit por arista, exclusivamente de runtime.** Mantendría intactos el reparto energético, los autoconsumos y las sondas actuales.

El brief acierta al señalar que las entradas lógicas dependen del flujo, pero su explicación es incompleta:

- En **A3**, con A encendido y B apagado, el reparto no reduce a cero la petición de A: A pide los **10 u/s completos**. El cierre incorrecto aparece durante la subida de demanda a través del Splitter, que produce dos lecturas oscuras y agota la tolerancia de la puerta.
- En **D5**, el láser puede quedar atrapado pidiendo únicamente sus **5 u/s de autoconsumo**. Intervienen la exclusión por suministro actual cero y la publicación con `m_GateClosed` todavía desactualizado. La oferta potencial abierta no es necesariamente cero: en el circuito reconstruido vale **43**.
- Añadir sondas permanentes no resuelve por sí solo el apagón transitorio de A3. Leer directamente `m_OfferedResidual` tampoco proporciona una señal independiente de la demanda.

Este documento es una propuesta para reconciliación y R22. No autoriza implementación.

## 1. Alcance, base y evidencia obtenida

Se han leído completos `AGENTS.md`, `CLAUDE.md`, el brief y las mediciones indicadas. No se han consultado otros planes.

El checkout real es `claude/project-thread-0iolln`, commit `1c765f568c307a059099f7e454772621e017c3c4`. Su padre es `592ebcc`; la única diferencia entre ambos commits es la incorporación de `BRIEF-GRAFO.md`. Por tanto, el código examinado corresponde a la base solicitada.

Trabajo realizado exclusivamente en lectura y mediante cálculos en memoria:

| Comprobación | Resultado |
|---|---|
| `python -B -m unittest discover -s .github/tools -p 'test_graph_*.py'` | 98 tests, código 0 |
| `python -B .github/tools/test_memorycell_toggle.py` | 10 tests, código 0 |
| Testigo A3: dos apagones, callback real de XOR extraído del fuente | Rojo: cierra la puerta aunque A sigue encendido y B apagado |
| Testigo D5: preámbulo real de PDQ hasta `AllocateOutput` | Rojo: petición hacia Set = 0 en la primera pasada abierta |
| Oráculo: asignaciones factibles de A3 y D5 | Aceptadas |
| Oráculo: asignaciones deficitarias reconstruidas | Rechazadas |
| Estado final del árbol | Sin cambios |

Los testigos no ejecutan Enforce ni reproducen el planificador de DayZ. Confirman mecanismos y valores; no reproducen los 73 cambios en 60 segundos ni acreditan una corrección en partida.

No se han escrito tests, informes, código ni commits en disco.

## 2. Contraste de las hipótesis del brief

| Hipótesis | Dictamen y evidencia |
|---|---|
| Las entradas lógicas leen flujo asignado. | **Confirmada.** `IsPortReceivingPower` filtra aristas habilitadas y puerto, y devuelve verdadero si `m_AllocatedPower > LFPG_PROPAGATION_EPSILON`: `scripts/5_Mission/LFPG_ElecGraphImpl.c:5037`, `:5040`, `:5044`. AND/OR/XOR lo consultan en `scripts/4_World/LFPG_LogicGate.c:141`. Memory Cell consulta sus cuatro entradas en `scripts/4_World/LFPG_MemoryCell.c:205`. |
| Un PASSTHROUGH publica demanda mediante `m_LastStableOutput`. | **Confirmada.** Se utiliza como demanda del destino en `scripts/5_Mission/LFPG_ElecGraphImpl.c:4605`; PDQ publica autoconsumo más demanda aguas abajo en `:2565` y `:2575`, y escribe el resultado en `:2789`. No representa necesariamente potencia entregada. |
| Water-fill deja una entrada lógica sin flujo y eso explica todos los casos. | **Parcial. No explica A3 por sí sola.** B cerrado no es proveedor activo. En la pasada de A, A entra por `fromSelf`, B queda excluido y A conserva una petición de 10. Véanse `:4009`, `:4022`, `:4382` y `:4411`. El cero posterior procede de la asignación binaria al disponer A todavía de 1. |
| La sonda desaparece cuando abre la puerta. | **Confirmada, con matiz.** El override a `max(sonda, autoconsumo)` pertenece al caso cerrado: `:2658–2671`. El otro uso citado en el brief es una estimación de arranque para demanda desconocida, no una reserva permanente por puerto: `:4608`, `:4634–4648`. |
| Sin Splitter funciona porque cambian los proveedores del merger. | **Cae como explicación diferencial.** Los proveedores inmediatos de la puerta son A y B en ambos montajes. Lo que cambia es que el generador puede satisfacer directamente la nueva petición usando su capacidad fija; el Splitter dispone inicialmente solo de la asignación anterior y añade otro apagón durante la subida. |
| La oferta del láser es necesariamente `5 − 5 = 0`. | **Cae para la oferta residual potencial.** `PassthroughSupplyAt` sí utiliza entrada asignada menos autoconsumo: `:4028–4033`. Sin embargo, `ComputeOfferBasePassthrough` utiliza **ofertas entrantes**, no `m_InputPower`: `:4080`, mediante `IncomingOfferSumForBase` en `:4220`. En D5 abierto resulta `48 − 5 = 43`. |
| El láser no puede recuperar porque queda excluido. | **Incompleta.** Otros proveedores lo excluyen cuando su suministro actual es cero, pero su propia pasada lo incluye siempre: `:4011–4013`. Una segunda pasada forzada, ya con la compuerta cacheada abierta, pide 0,5 adicionales. La primera puede publicar cero porque usa el estado cerrado anterior. |
| TA3-01 son dos constantes de umbral distintas. | **No.** Ambas comparaciones usan `0.001`; difieren los predicados: nivel absoluto en `:5044`, variación en `:4793`. Un cruce pequeño puede cambiar el booleano sin superar el umbral de variación. No se propone corregir ese comportamiento general. |

Otra precisión: los 400 ms de Memory Cell se aplican a **Power y Toggle**, no a Set y Reset. Las temporizaciones están en `scripts/4_World/LFPG_MemoryCell.c:213–225`; Set y Reset actúan por nivel en `:235–241`.

## 3. Mecanismo común en `ProcessDirtyQueue`

El orden relevante actual es:

1. Sumar potencia asignada de entradas habilitadas mediante `GetEdgeAllocatedPower`: `scripts/5_Mission/LFPG_ElecGraphImpl.c:2315–2353`.
2. Calcular disponibilidad real, restando autoconsumo: `:2379–2399`.
3. Leer la compuerta viva de la entidad y decidir `allocAvail`: `:2483–2523`.
4. Ejecutar `AllocateOutput`: `:2525`.
5. Publicar demanda y actualizar **después** `node.m_GateClosed`: `:2549–2671`.
6. Propagar cambios y permitir reprocesamiento: `:2786–2866`.
7. Sincronizar la entidad: `:2884`, que llama a `SetPowered` en `:3207`.
8. El callback lógico vuelve a leer las asignaciones y puede cambiar su latch y solicitar otra propagación: `scripts/4_World/LFPG_LogicGate.c:141–145`, `:186–219`.

Hay dos acoplamientos:

- Una asignación temporalmente cero modifica una decisión lógica que, a su vez, modifica la demanda.
- La oferta y el vector de proveedores dependen de estados que pueden no haberse publicado todavía de forma coherente durante una transición.

El arreglo propuesto elimina el primer acoplamiento para las cinco entradas de señal. No necesita cambiar las reglas económicas para resolver el segundo síntoma.

## 4. A3: recorrido numérico completo del mecanismo

### 4.1 Circuito y notación

```text
G, 50
  └─ GS ─ S, Splitter 200
             ├─ SA ─ A, encendido ─ AX ─ XOR.input_0
             └─ SB ─ B, apagado   ─ BX ─ XOR.input_1
                                      XOR ─ XL ─ lámpara
```

Valores verificados:

- Splitter: consumo 0, capacidad 200: `scripts/4_World/LFPG_Splitter.c:68–70`.
- Interruptores: consumo 0 y compuerta igual a `m_SwitchOn`: `scripts/4_World/LFPG_SwitchDeviceBase.c:26–32`.
- Puerta: consumo 0, capacidad 100: `scripts/4_World/LFPG_LogicGate.c:18`, `:109–114`.
- XOR: `scripts/4_World/LFPG_LogicGate.c:377–387`.
- CeilingLight es **PASSTHROUGH**, consumo 10 y capacidad 50, no CONSUMER: `scripts/4_World/LFPG_LampDeviceBase.c:37–39`.

En las tablas, `d/a` significa **demanda almacenada en la arista / asignación**. `D(X)` es la demanda que publica el nodo XOR aguas arriba.

El estado inicial representa una puerta que acaba de abrir su latch al leer `(1,0)`, mientras la asignación aún corresponde a la sonda cerrada. Todas las demandas PASSTHROUGH del testigo son conocidas: no interviene el fallback de capacidad.

### 4.2 Secuencia con Splitter

Es un orden local válido del mecanismo reconstruido, no una transcripción del orden interno de la partida.

| Paso / nodo procesado | GS d/a | SA d/a | SB d/a | AX d/a | BX d/a | XL d/a | Resultado |
|---|---:|---:|---:|---:|---:|---:|---|
| 0. Estado inicial | 2/2 | 1/1 | 1/1 | 1/1 | 0/0 | 10/0 | XOR acaba de abrir con `(1,0)` |
| 1. XOR | 2/2 | 1/1 | 1/1 | 1/1 | 0/0 | 10/0 | Entrada 1; pide 10; no puede entregarlos; `D(X)=10` |
| 2. A | 2/2 | 1/1 | 1/1 | 10/0 | 0/0 | 10/0 | Pide 10, dispone de 1: all-off; publica 10 |
| 3. B | 2/2 | 1/1 | 1/1 | 10/0 | 0/0 | 10/0 | Cerrado; publica sonda 1 |
| 4. Lámpara | 2/2 | 1/1 | 1/1 | 10/0 | 0/0 | 10/0 | Entrada 0; apagada |
| 5. XOR | 2/2 | 1/1 | 1/1 | 10/0 | 0/0 | 10/0 | Primer `(0,0)`; conserva latch abierto |
| 6. Splitter | 2/2 | 10/0 | 1/0 | 10/0 | 0/0 | 10/0 | Pide `10+1=11`, dispone de 2: all-off; publica 11 |
| 7. Lámpara | 2/2 | 10/0 | 1/0 | 10/0 | 0/0 | 10/0 | Sigue apagada |
| 8. A | 2/2 | 10/0 | 1/0 | 10/0 | 0/0 | 10/0 | Ahora recibe 0; mantiene demanda 10 |
| 9. B | 2/2 | 10/0 | 1/0 | 10/0 | 10/0 | 10/0 | A tampoco aparece activo; B conserva petición bruta 10, pero cerrado publica 1 |
| 10. Generador | 11/11 | 10/0 | 1/0 | 10/0 | 10/0 | 10/0 | Puede satisfacer 11 porque dispone de 50 |
| 11. XOR | 11/11 | 10/0 | 1/0 | 10/0 | 10/0 | 10/0 | Segundo `(0,0)`: **cierra el latch incorrectamente** |
| 12. Splitter | 11/11 | 10/10 | 1/1 | 10/0 | 10/0 | 10/0 | Ya dispone de 11 y reparte 10+1 |
| 13. XOR | 11/11 | 10/10 | 1/1 | 10/0 | 10/0 | 10/0 | Compuerta cerrada: `D(X)` vuelve a 1 |
| 14. A | 11/11 | 10/10 | 1/1 | 1/1 | 10/0 | 10/0 | Sustituye petición 10 por 1 |
| 15. B | 11/11 | 10/10 | 1/1 | 1/1 | 0/0 | 10/0 | Sigue cerrado; publica 1 |

Cuando vuelva a procesarse XOR con AX=1, abrirá otra vez. La caída de demanda vuelve después hacia Splitter y generador, preparando otra subida.

La tolerancia que se agota está implementada en `scripts/4_World/LFPG_LogicGate.c:166–189`: solo conserva el latch durante **una** evaluación oscura consecutiva.

La decisión all-off está en `scripts/5_Mission/LFPG_ElecGraphImpl.c:4735–4785`. Es la causa inmediata del cero; no es un reparto de 10 entre A y B.

### 4.3 Qué hace realmente water-fill aquí

En la pasada de A:

- A entra aunque su suministro sea cero, por `fromSelf`.
- B cerrado tiene suministro cero y queda excluido.
- El vector tiene un único participante.
- `ApplyMergerWaterFill` no reescribe la demanda.
- AX pide 10 y recibe 0 porque A solo dispone de 1.

Además, la oferta residual de A puede seguir siendo **49** mientras AX tiene asignación cero:

```text
oferta G→S = 50
oferta S→A = 50 − demanda de B = 50 − 1 = 49
oferta A→XOR = 49
asignación A→XOR = 0
```

Esto se comprobó con los helpers extraídos del fuente.

### 4.4 Por qué sin Splitter sí funciona

Se sustituyen GS, SA y SB por GA y GB:

| Paso | GA d/a | GB d/a | AX d/a | BX d/a | XL d/a | Resultado |
|---|---:|---:|---:|---:|---:|---|
| Inicial | 1/1 | 1/1 | 1/1 | 0/0 | 10/0 | XOR abre |
| XOR pide carga | 1/1 | 1/1 | 1/1 | 0/0 | 10/0 | Publica 10 |
| A | 1/1 | 1/1 | 10/0 | 0/0 | 10/0 | Primer apagón |
| XOR | 1/1 | 1/1 | 10/0 | 0/0 | 10/0 | Conserva latch durante esa evaluación |
| Generador | 10/10 | 1/1 | 10/0 | 0/0 | 10/0 | Satisface directamente `10+1=11 ≤ 50` |
| A | 10/10 | 1/1 | 10/10 | 0/0 | 10/0 | Recupera AX |
| XOR | 10/10 | 1/1 | 10/10 | 0/0 | 10/10 | Lee `(1,0)`, mantiene apertura y alimenta lámpara |

Falta el segundo distribuidor que todavía conserva una entrada de 2. La tolerancia existente basta para esta secuencia.

**Límite de la demostración:** el modelo reducido puede converger con ciertos órdenes; en una ejecución reconstruida convergió en tres épocas. No demuestra la frecuencia ni la persistencia de la oscilación observada en DayZ. Sí demuestra el cierre incorrecto durante una entrada física constante y la diferencia causal introducida por Splitter.

### 4.5 A4, sin carga

Con XOR abierto y sin cable de salida:

- Demanda aguas abajo = 0.
- Autoconsumo = 0.
- PDQ publica cero en `scripts/5_Mission/LFPG_ElecGraphImpl.c:2625–2629`.
- AX puede recibir cero aunque A siga abierto y conectado a una fuente.
- Dos lecturas oscuras vuelven a cerrar XOR.
- Cerrado, reaparece la sonda 1 y puede volver a abrir.

Es otro mecanismo de realimentación entre lectura lógica y demanda. No depende de que una lámpara pida demasiada potencia.

## 5. D5: recorrido numérico y variantes

### 5.1 Circuito completo con Power por Splitter

```text
G, 50
 ├─ GX ─ Laser, consumo 5 ─ XM ─ Memory.Set
 ├─ GS ─ Splitter ─ SM ───────── Memory.Power
 └─ GR ─ pulsador Reset ─ RM ─── Memory.Reset

Memory.Output ─ ML ─ lámpara, consumo 10
```

Memory Cell consume 1 y tiene capacidad 100: `scripts/4_World/LFPG_MemoryCell.c:20–21`.

Antes del cruce:

- Láser alimentado, compuerta cerrada.
- Reset cerrado.
- Latch falso y ML deshabilitada mediante `LFPG_ApplyRouting`: `scripts/4_World/LFPG_MemoryCell.c:419–422`.
- Demanda de Memory = 1.
- GX=5, GS=1 y GR=1: total 7.

Las ofertas escritas por G son:

```text
G→Laser = 50 − 1 − 1 = 48
G→Splitter = 50 − 5 − 1 = 44
G→Reset = 50 − 5 − 1 = 44
```

Splitter transmite una oferta de 44 hacia Power.

### 5.2 Primera pasada después de abrir el láser

La entidad ya devuelve compuerta abierta. PDQ obtiene correctamente `gateIsClosed=false`, pero todavía no ha actualizado `node.m_GateClosed`.

| Paso | GX d/a | GS d/a | GR d/a | XM d/a | SM d/a | RM d/a | ML efectiva | Resultado |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Inicial | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0, deshabilitada | Memory inactiva |
| Láser: Step 1–2 | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0 | Entrada 5; disponible `5−5=0` |
| Láser: `AllocateOutput` | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0 | Vector incluye láser propio y Power; oferta propia aún 0 por caché cerrada |
| Láser: publicación | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0 | Publica demanda `5+0=5` y oferta XM=0 |
| Láser: post-gate | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0 | Ahora actualiza `m_GateClosed=false`; marca aguas arriba y abajo |
| Memory | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0 | Power alto, Set bajo; conserva latch falso y demanda 1 |
| Splitter | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0 | Excluye láser: suministro actual `5−5=0`; cubre Power completo |
| Generador | 5/5 | 1/1 | 1/1 | 0/0 | 1/1 | 0/0 | 0 | Demanda sigue siendo 7; asignaciones y ofertas no cambian |

La oferta potencial que tendría el láser **si volviera a publicarse abierto** es:

```text
ComputeOfferBasePassthrough(100, 48, 0, 5, false) = 43
```

Pero la oferta escrita sigue siendo cero. No hay una garantía de otra pasada propia:

- El cambio de compuerta marca proveedores en `scripts/5_Mission/LFPG_ElecGraphImpl.c:2696–2706`.
- Step 3 marca destinos y proveedores, no solicita automáticamente una segunda pasada propia.
- G puede procesarse sin cambiar ninguna asignación ni oferta.
- Memory continúa pidiendo 1, cubierta por Power.

`RefreshSourceState` tampoco actualiza `m_GateClosed` para PASSTHROUGH: actualiza capacidad y consumo en `:3750–3769`.

### 5.3 Discriminadores ejecutados

Con los mismos helpers del fuente:

| Experimento en memoria | Resultado |
|---|---|
| Primera pasada abierta con caché cerrada | Petición XM=0; demanda del láser=5 |
| Forzar otra pasada propia, ya con caché abierta | Petición XM=0,5; demanda del láser=5,5 |
| Retirar Power y dejar el láser como único proveedor efectivo | Petición XM=1; demanda del láser=6 |

La segunda fila tumba la afirmación de que el láser siempre ofrecerá cero por recibir solamente 5. Su pasada propia puede arrancar la recuperación si utiliza la oferta abierta.

### 5.4 Variantes medidas

- **Power directo desde G:** sustituir GS+SM por GM mantiene los mismos números iniciales y el mismo bloqueo. Las mediciones de D5 ya incluyen este fallo sin Splitter. No existe un control positivo «D5 sin Splitter funciona» que deba explicarse.
- **Sin Power:** el láser es el único proveedor efectivo; no se reparte la demanda 1 de Memory y aparece una petición adicional. Después de Set, la demanda puede crecer hasta 11 y alimentar la lámpara. Al cerrar el láser desaparece esa alimentación, conservándose el latch.
- **Interruptor normal hacia Set:** consumo cero; una entrada de sonda 1 deja suministro actual positivo. Puede participar en el reparto sin necesitar superar primero un autoconsumo de 5.
- **Láser → interruptor abierto → Set:** el interruptor intermedio recibe cero y puede publicar demanda conocida cero. Su oferta entrante escrita también es cero. Power cubre Memory y falta el evento que haga avanzar la cadena desde el láser. Añadir otro PASSTHROUGH no rompe el bloqueo.

## 6. Dirección elegida y alternativas descartadas

### 6.1 Sonda permanente por entrada

Puede evitar la desaparición de flujo en A4 y forzar una petición adicional hacia el láser de D5, **si se aplica después del reparto**. Aplicarla antes permite que el reparto vuelva a eliminarla.

No basta para A3:

- `max(1, parte)` deja la petición de A en 10.
- Una sonda aditiva la elevaría a 11.
- Splitter sigue disponiendo inicialmente de 2 y aplicando all-off.
- La puerta sigue evaluando los apagones durante la subida.

También añade demanda cerca del límite y obliga a definir cómo representa el oráculo la energía de sondeo sobrante.

### 6.2 Leer directamente la oferta residual

Resuelve algunos ceros de flujo, pero conserva una dependencia de la demanda. Se comprobó:

```text
Splitter con base 50, peticiones 50 + 50:
    oferta hacia cada rama = 50 − 50 = 0

Tras cerrar la puerta, peticiones 0,5 + 0,5:
    oferta hacia cada rama = 50 − 0,5 = 49,5
```

Cambiar la consulta a `m_OfferedResidual > EPSILON` puede trasladar la realimentación a ese campo. También exigiría propagar ofertas por tramos sin flujo: `NotifyOfferChanged` solo despierta actualmente ciertos destinos, según el lookahead de `scripts/5_Mission/LFPG_ElecGraphImpl.c:4263–4283`.

### 6.3 Propuesta: señal de control explícita

**[DESIGN]** Añadir un booleano de runtime por arista, separado de demanda, asignación y oferta residual.

Contrato propuesto:

| Productor | Señal publicada en una salida habilitada |
|---|---|
| SOURCE del enum | Encendida y con capacidad positiva |
| PASSTHROUGH sin autoconsumo | Hay señal entrante o generación virtual positiva, y la compuerta está abierta |
| PASSTHROUGH con autoconsumo | Lo anterior, y su alimentación real cubre su autoconsumo |
| Compuerta cerrada o salida deshabilitada | Baja |

«SOURCE» significa `LFPG_DeviceType.SOURCE`. No se debe usar `LFPG_IsSource()`: interruptores y puertas también devuelven verdadero pese a ser PASSTHROUGH.

Las únicas lecturas que pasan a este canal son:

- AND/OR/XOR: `input_0` e `input_1`.
- Memory Cell: Toggle, Reset y Set.
- **Memory.Power continúa leyendo flujo asignado.**

Una señal alta no certificará energía entregada. En particular:

- Un interruptor abierto puede transmitir señal durante el all-off transitorio de un distribuidor.
- Una puerta sin carga puede permanecer lógicamente abierta con asignación cero.
- Un láser alimentado para su autoconsumo puede transmitir Set sin suministrar la energía de Memory.
- Una lámpara necesita seguir recibiendo sus 10 reales para encender.

Esta semántica es una **decisión propuesta**, no un hecho del código actual.

## 7. Cambios mínimos previstos y trazabilidad

Los nombres nuevos de esta sección son **[DESIGN]**.

| Archivo / ancla actual | Cambio previsto | Causa que resuelve |
|---|---|---|
| `scripts/3_Game/LFPG_Data.c:337–355` | Añadir `m_SignalHigh` a `LFPG_ElecEdge`, inicializado a falso | Representar señal sin sobrecargar `m_AllocatedPower` ni `m_OfferedResidual` |
| `scripts/4_World/LFPG_ElecGraph.c:305` | Declarar `IsPortReceivingSignal(deviceId, portName)`, con fallback falso | Hacer accesible la consulta desde World respetando el orden de módulos |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:81`, `:2298` | Añadir y reiniciar por nodo `m_SignalChanged` | Detectar cambios de señal incluso con demanda y asignación cero |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:2308–2353` | Acumular presencia de señal en el mismo recorrido de entradas habilitadas | Propagar por cadenas sin flujo, especialmente A4 y D5 |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:2483–2525` | Calcular señal de salida usando `gateIsClosed` recién leído, alimentación propia y señal entrante | Publicar correctamente la apertura del láser sin depender del caché económico anterior |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:4562`, `:4754–4796` | Pasar la señal a `AllocateOutput` y escribirla en el recorrido existente, fuera de `if (!overloaded)` | Mantener el canal lógico durante el all-off energético de A3 |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:2786`, `:2815` | Incluir `m_SignalChanged` en propagación y reprocesamiento del destino en la misma época | Evitar perder cambios de señal cuando los floats no cambian |
| `scripts/5_Mission/LFPG_ElecGraphImpl.c:5013` | Implementar la nueva consulta con filtros de puerto, habilitación y contabilidad de visitas equivalentes | Leer el canal nuevo sin fallback energético |
| `scripts/4_World/LFPG_LogicGate.c:141–142` | Consultar señal mediante el grafo | Evitar que el apagón de reparto cierre la puerta |
| `scripts/4_World/LFPG_LogicGate.c:80–84`, `:148–189` | Retirar la tolerancia de una lectura oscura y evaluar directamente la señal | Eliminar el parche dependiente del número de callbacks y permitir apagado lógico inmediato |
| `scripts/4_World/LFPG_MemoryCell.c:205–208` | Mantener Power por flujo; cambiar únicamente Toggle/Reset/Set a señal | Resolver D5 y conservar el contrato de Power |

`LFPG_NetworkManager.GetGraph()` ya está declarado en `scripts/4_World/LFPG_NetworkManager.c:250` e implementado en `scripts/5_Mission/LFPG_NetworkManagerImpl.c:892`. No hace falta añadir otro wrapper al manager.

Se conservarán:

- El `RequestPropagate` existente cuando cambia el latch de la puerta.
- Los límites y la cola diferida.
- La política de conservar el último estado de compuerta si no se resuelve la entidad.
- Los filtros de arista habilitada. Deshabilitar una salida no debe provocar escrituras anticipadas sobre asignaciones; el motivo está documentado en `scripts/5_Mission/LFPG_ElecGraphImpl.c:5083–5089`.

No es necesario mover la actualización económica de `m_GateClosed` ni corregir su bootstrap para solucionar D5 con este diseño. El nuevo canal utiliza la instantánea viva ya obtenida antes de la asignación.

### Resultado energético esperado

A3 puede estabilizarse con:

```text
GS=11, SA=10, SB=1, AX=10, BX=0, XL=10
```

D5 puede estabilizarse sin aportar flujo por Set:

```text
GX=5, GS=11, GR=1, XM=0, SM=11, RM=0, ML=10
```

Total de G: `5+11+1=17`.

El oráculo aceptó ambas asignaciones. La segunda demuestra que resolver D5 no requiere obligar al láser a financiar parte de la celda.

## 8. Reproducción offline y contrato de tests

### 8.1 Fichero previsto

**[DESIGN]** `.github/tools/test_graph_logic_signals.py`.

Se escribirá primero, antes del cambio de producción, y deberá dar rojo funcional sobre `592ebcc`. Aún no existe; no se presenta como ejecutado.

Comandos previstos:

```powershell
python -B .github/tools/test_graph_logic_signals.py
python -B -m unittest discover -s .github/tools -p 'test_graph_*.py'
python -B .github/tools/test_memorycell_toggle.py
```

Códigos:

- **0:** todas las aserciones pasan.
- **1:** fallo funcional o excepción del test; distinguir ambos en el informe.
- Fichero ausente, cero tests descubiertos o fallo de extracción: **instrumentación inválida**, no reproducción válida del bug.

### 8.2 Herramientas existentes que se reutilizan

- Construcción de nodos y aristas de `QueueModel.add_node/add_edge`: `.github/tools/test_graph_multifeed_split.py:109`, `:137`.
- Helpers económicos extraídos del fuente: `:28–44`.
- Cola como punto de partida, **no como emulador completo de PDQ**.
- Extracción y ejecución escalar: `.github/tools/enforce_scalar_slice.py:9`, `:22`, `:75`.
- Técnica de enlace de callbacks reales: `.github/tools/test_memorycell_toggle.py:82`, `:178–205`.
- `graph_reference.Graph`, `Node`, `Edge` y `Oracle.verify`: modelo en `.github/tools/graph_reference/model.py`; verificación en `.github/tools/graph_reference/oracle.py:267`.

`enforce_scalar_slice` rechaza bucles y construcciones no soportadas en `:26`. No se afirmará que ejecuta PDQ entero.

El driver del nuevo test deberá:

1. Modelar explícitamente cola, compuertas y routing.
2. Ejecutar los helpers escalares y callbacks reales que pueda extraer.
3. Enlazar la consulta realmente utilizada por esos callbacks; no sustituirla siempre por un lector ideal del test.
4. Hacer fallar la extracción si cambia una estructura no soportada.
5. Separar la lógica esperada —tabla de verdad y órdenes del circuito— del estado observado del solver.

`QueueModel.process` no implementa actualmente los callbacks completos de las puertas ni el override de demanda cerrada; `run` tampoco acredita convergencia por agotar su presupuesto de pasos. Véanse `.github/tools/test_graph_multifeed_split.py:397–472`.

### 8.3 Fixture A3 y rojo actual

Construcción exacta:

- G: SOURCE 50.
- S: PASSTHROUGH 200, consumo 0.
- A y B: PASSTHROUGH 200, consumo 0; A abierto y B cerrado.
- XOR: PASSTHROUGH 100, consumo 0.
- L: PASSTHROUGH 50, consumo 10, sin salida conectada.
- Aristas y puertos según el apartado 4.
- Demandas conocidas; estado de sonda inicial y ofertas sembradas según la tabla.
- Dos visitas oscuras producidas por las pasadas A y Splitter.
- Callback `LFPG_SetPowered` y función XOR extraídos del fuente.

Test previsto:

```text
test_a3_shared_splitter_two_dark_visits
```

Aserción que da rojo hoy:

```python
self.assertTrue(gate.m_GateOpen)
```

Valor actual observado: falso tras la segunda lectura oscura, pese a mantener A encendido, B apagado y oferta AX=49.

Con el arreglo, el test exigirá que AX conserve señal alta durante ambas asignaciones cero. Después drenará la cola y comprobará la asignación final factible y lámpara encendida.

Control obligatorio: montaje directo GA/GB, que atraviesa una sola lectura oscura y recupera 10.

### 8.4 Fixture D5 y rojo actual

Construcción exacta:

- G50; Laser100/consumo5; Splitter200/consumo0.
- Memory100/consumo1; Reset cerrado; lámpara50/consumo10.
- Todas las aristas del apartado 5, incluida ML inicialmente deshabilitada.
- Power alto, latch inicialmente falso.
- Evento externo: láser abre su compuerta, manteniendo alimentación propia 5.

Tests previstos:

```text
test_d5_laser_set_with_separate_power
test_d5_laser_set_through_open_switch
test_d5_direct_power_has_same_failure
```

Aserciones de aceptación:

- Set se lee alto después de propagarse la apertura.
- El callback real de Memory pone el latch a verdadero.
- El routing habilita Output.
- La lámpara recibe 10 después de converger.
- Power conserva su lectura energética.
- Se admite `XM.m_AllocatedPower == 0`.

Hoy la primera aserción falla: Set se lee bajo porque XM tiene asignación cero.

La sonda ejecutada en esta fase comprobó además que la petición de XM es cero en la primera pasada abierta. Ese resultado sirve para aislar el mecanismo económico; **el arreglo propuesto no necesita convertirlo en una petición positiva**. El criterio de regresión es Set/latch/lámpara, no gastar energía por el puerto Set.

### 8.5 Uso independiente del oráculo

El oráculo actual no contiene puertos lógicos ni tablas de verdad. No puede certificar por sí solo que XOR o Set están bien.

Se utilizará para capacidad, conservación y factibilidad, con estas precauciones:

- La apertura lógica esperada se obtiene de las órdenes de los interruptores y la tabla de verdad, no del latch defectuoso observado.
- No se entregará al oráculo una puerta incorrectamente cerrada y se aceptará después que la lámpara sea «inalcanzable».
- Las sondas cerradas existentes de B y Reset se representarán explícitamente como consumo de sondeo en la fixture. Eso no cambia el consumo declarado del dispositivo en producción.
- Las asignaciones deficitarias deben producir, según el estado, `hard_unmet`, `conservation` y `feasible_but_underfed`.

Las dos asignaciones positivas del apartado 7 ya fueron aceptadas por el oráculo. Las asignaciones deficitarias reconstruidas fueron rechazadas.

### 8.6 Positivos, negativos y mutantes

| Familia | Fixture positiva | Negativo o mutante que debe morder |
|---|---|---|
| A1–A3 | Tablas AND `0001`, OR `0111`, XOR `0110`, con y sin Splitter | Volver a leer asignación; A3 vuelve a cerrar durante la subida |
| A4 | XOR sin carga, A encendido, 600 ticks simulados de 100 ms | No transmitir señal si asignación=0; reaparece la realimentación |
| B1–B2 | Cambios de XOR hacia Door Controller | Publicar señal solo dentro de `!overloaded` |
| D5 | Láser abierto, Power separado, Set alto y lámpara 10 | Usar el `m_GateClosed` anterior para publicar señal |
| Cadena D5 | Láser → interruptor abierto → Set | No propagar un cambio de señal cuando demanda/asignación no cambian |
| Corte real | Apagar G, cerrar A, cortar cable y deshabilitar salida | Retener señal previa o ignorar `LFPG_EDGE_ENABLED` |
| Arranque | Aristas nuevas a señal falsa, warmup y reconstrucción | Inicializar señal a verdadera |
| Power | Señal presente y flujo Power=0 | Cambiar también Power a la nueva consulta; debe fallar el permiso de Toggle |
| Sensor sin alimentación propia | Láser por debajo de 5 no transmite señal | Omitir el requisito de autoconsumo cubierto |
| Convergencia | Reprocesamiento del destino en la misma época | Omitir `m_SignalChanged` del reset de época o de la propagación |
| Sobrecarga | Fuente50, carga100: asignaciones all-off y señales de interruptores estables | Acoplar señal a `m_Overloaded` en los PASSTHROUGH de consumo cero |
| Memory | C1–C7, temporizadores puntuales y tardíos, reconexión/carga | Lectura por flujo de Toggle/Set/Reset o cambio de sus temporizaciones |

Un mutante cuenta como detectado cuando falla una aserción del comportamiento esperado. Un error de parser, método ausente o excepción del harness no contará como detección funcional.

### 8.7 Convergencia y límites del driver

- Leer del fuente los presupuestos de producción: 64 nodos, 256 visitas de arista, requeue máximo 5 y tick 100 ms: `scripts/3_Game/LFPG_Defines.c:465`, `:471`, `:494`, `:534`.
- Probar órdenes de conexión y de procesamiento distintos.
- Exigir cola y diferidos vacíos; no aprobar por alcanzar `steps`.
- Límite de ensayo: 100 épocas por transición. Agotarlo es FAIL de convergencia del modelo.
- A4: tras estabilización, como máximo un cambio en 600 ticks.
- Verificar que el reinicio por época conserva los cambios pendientes al agotar presupuesto.
- Para fixtures no lógicas, comparar todos los valores energéticos finales contra la base, no solo si el oráculo considera factible la solución.

## 9. Riesgos del brief, tratados individualmente

| Riesgo | Tratamiento y prueba |
|---|---|
| **Asignación binaria y B5 cerca del límite** | No se añade demanda de sonda ni se modifica all-off. Publicar el bit fuera de `!overloaded` es obligatorio para los tramos de consumo cero. Probar carga exactamente cubierta, superior al límite considerando EPSILON y muy superior al límite; la señal no debe financiar energía ni encender consumidores sin asignación. |
| **Regresión en Splitter, Combiner, baterías y consumidores** | Las fórmulas energéticas permanecen iguales. Las fixtures existentes deben conservar su vector energético final dentro de EPSILON, incluidas variantes de conexión, corte y recuperación de multi-feed. Ningún cambio de fixture existente se acepta sin explicar una diferencia concreta. |
| **Convergencia y requeue en la misma época** | Booleano de cambio separado, reiniciado por nodo, incluido en propagación y reset del destino ya procesado. Conservar límites, deduplicación y diferidos actuales. Probar especialmente cambios de señal con todos los floats constantes a cero. |
| **Coste por tick** | Leer el bit en el recorrido de entradas ya existente y escribirlo en el recorrido de asignación existente. Un booleano por arista y una bandera por grafo; sin listas, mapas, temporizadores ni búsquedas de entidades nuevos por tick. Medir visitas y reprocesamientos añadidos; la suite de probes estructurales no sustituye esa medición. |
| **Persistencia** | `LFPG_ElecEdge` es runtime; el propio campo de oferta lo documenta en `scripts/3_Game/LFPG_Data.c:340–343`. El bit arranca falso y se reconstruye por propagación. No se escribe en WireData, saves ni JSON persistente. Probar reconstrucción y arranque. |
| **Sincronización** | Conservar exactamente registros y orden de SyncVars. El bit no se replica: las entidades siguen enviando sus booleanos actuales. Una llegada de señal debe producir `DIRTY_INPUT`, que impide que `NodeEntitySyncUnchanged` suprima la lectura: `scripts/5_Mission/LFPG_ElecGraphImpl.c:2985`. |
| **TA3-01** | No cambiar `IsPortReceivingPower`, EPSILON ni el criterio de `m_AllocChanged`. El canal nuevo propaga diferencias booleanas exactas. No usar como oportunidad para corregir el comportamiento general de pequeñas variaciones energéticas. |

## 10. Suposiciones pendientes y pruebas que las tumbarían

| Suposición o decisión propuesta | Prueba que la tumbaría |
|---|---|
| Las entradas de control deben conservar señal durante un apagón de reparto de un distribuidor de consumo cero. | Un criterio de producto o prueba aceptada que exija leer esa entrada baja exclusivamente por asignación cero. Cambiaría la semántica propuesta y exigiría reconciliación antes de implementar. |
| El autoconsumo real cubierto es condición adecuada para que un sensor transmita señal. | Láser alimentado con menos de 5 que deba transmitir válidamente Set. Probar por debajo, en el umbral y por encima. |
| Separar la señal elimina el cierre incorrecto de A3 y permite D5 completo. | Los tests cerrados que usan callbacks reales siguen cerrando XOR, no activan Memory o no alimentan la lámpara tras drenar la cola. |
| Los cambios adicionales de scheduling no alteran reparto no lógico. | Cualquier fixture existente termina con un vector energético distinto de la base, fuera de EPSILON. No basta con que ambas soluciones sean factibles. |
| El coste añadido queda contenido en los recorridos y presupuestos existentes. | Crecimiento superlineal, nuevas asignaciones por tick, barridos ajenos a la propagación o diferidos que no se drenan. |
| El bit se reconstruye sin afectar saves o red. | Necesidad de serializarlo, cambiar WireData/SyncVars o pérdida persistente de señal tras reconstrucción, carga o reconexión. |
| El modelo offline representa los mecanismos relevantes del motor. | Un arranque o prueba en partida contradice la secuencia o el comportamiento. En tal caso prevalece la evidencia del juego y se reabre el diagnóstico. |

## 11. Qué no se toca

- Demanda hard/soft, prioridades, reparto de excedentes y generación virtual.
- `ApplyMergerWaterFill`, `IncludeInMergerVector`, `PassthroughSupplyAt` y las fórmulas de oferta residual.
- `m_LastStableOutput`, `m_DemandKnown` y las reglas de cold-start.
- Política all-off, EPSILON, presupuestos y límite de requeue.
- Filtro de 400 ms, rearme de 1000 ms y gracia de carga de Memory Cell.
- Prioridad actual de Set/Reset: ambos altos conservan el estado; no se inventa prioridad de Reset.
- Detección del láser, vehículos, geometría, haz y hold.
- Electronic Counter y los demás consumidores de `IsPortReceivingPower`.
- Classnames, `config.cpp`, modelos, layouts, textos, RPC y formatos persistentes.
- Registros u orden de SyncVars.
- Arquitectura pública de `graph_reference` ni expectativas energéticas existentes para acomodar diferencias.
- Commits, ramas, empaquetado, PBOs o arranque del juego durante esta fase.

## 12. Puertas de validación y entrega posterior

1. **Reconciliación de planes y R22:** aprobar explícitamente el contrato de señal y el alcance de cinco archivos de producción.
2. **Tests primero:** crear la reproducción propuesta y obtener rojos funcionales sobre `592ebcc`. Conservar evidencia de las aserciones, no solo el código de salida.
3. **Implementación por el escritor asignado:** cambios localizados, CRLF y convenciones de `AGENTS.md`; sin refactor adyacente.
4. **Offline:** nuevo test, todos los `test_graph_*.py`, `test_memorycell_toggle.py`, oráculo y mutantes en verde. Comparación energética contra la base sin diferencias no justificadas.
5. **Linter obligatorio:** ejecutar sobre base y árbol modificado, comparando los JSON por sus listas raíz `errors` y `warnings`:

   ```powershell
   python -B C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .
   ```

   Cero errores nuevos. El código 2 por warnings no se interpreta automáticamente como fallo. Comprobar además cada miembro nuevo contra su declaración real; el linter no acredita compilación Enforce.

6. **Revisión R21:** causas, límites, mutantes y diffs de reparto. No aprobar únicamente por número de tests.
7. **Partida, en la fase autorizada:** comprobar hash del PBO desplegado frente a BASE `592ebcc` y ejecutar la matriz del brief:
   - A1–A3 con Splitter: tablas correctas en menos de 3 s.
   - A4: 0 o 1 cambios en 60 s.
   - B1 invierte la puerta en cada cambio; cubrir B2.
   - D5 activa y conserva Memory; Reset la desactiva después de retirar Set y expirar el hold del láser.
   - C1–C7 siguen pasando.
   - Regresión sin Splitter, D1–D4 y G1–G2.
   - Banco grande si está disponible.
   - Ningún `SCRIPT (E)` nuevo y como máximo un `Leaked 'LFPG_` al cerrar.
   - Restaurar el banco.

**Estado de esta fase:** diagnóstico de los mecanismos sustentado por código y testigos numéricos; propuesta pendiente de reconciliación. La nueva implementación y su comportamiento en DayZ siguen sin verificar.
