# Fase 3 en partida: arreglo del grafo (bf2cb33) frente a BASE (592ebcc)

Fecha: 2026-11-09. Banco `dayzOffline.lfpg_grafo.chernarusplus` con `@LFPowerGrid_grafo`, driver gs01
(operaciones del servidor) y el lanzador dayz-mcp. El juego no se lanzó contra el servidor de jugadores.

## PBO

| Uso | Árbol | sha256 |
|---|---|---|
| Matriz, con el arreglo | bf2cb33 | 6C18FC2F39A5DF6793E36F45624FC73A9BC34237505FE3C1CD01FD7240B8F3CB |
| Matriz, BASE | 592ebcc | 816DE372237E4894E7CBC84B56F2A58628C9152DCAD5D3B004A0E4A54BFEF4E6 |
| Trazas (LOG_LEVEL 2 y DIAG_PT_CHAIN), arreglo | bf2cb33 | 0E2B666F033097C28126CDB67ED327CCAE237F5F91332B8A58FAAF9F0B3F0F0D |
| Trazas, BASE | 592ebcc | 38419126C002F11A… |
| LFPG_PERF_PROBE, arreglo | bf2cb33 | 4BA8AB22FE37BEE0216FD350D4A1F5389C60028C9EB31BC91A5EFBB903EF27A5 |
| LFPG_PERF_PROBE, BASE | 592ebcc | 1EAE584511C11669D4376F77042FE01BFB665C9D4AF34B13996073FE48E74A77 |

Build aislado con `build_isolated_pbo.py --expected-p3d 51`; linter sobre el árbol del build del
arreglo: 0 errores, 48 warnings. Con el arreglo, servidor y cliente arrancan sin `SCRIPT (E)` en el RPT
ni errores en el log de script.

## Matriz del §8

| Caso | BASE 592ebcc | Arreglo bf2cb33 |
|---|---|---|
| A1-A3 con Splitter, tabla en secuencia sobre el mismo circuito | AND 0000, OR 0001, XOR 0000: FALLA | 0001, 0111, 0110: PASA |
| A4 (XOR, solo A, sin salida, 60 s) con Splitter | 67 cambios | 1 cambio |
| A4 sin Splitter | 31 cambios | 1 cambio |
| B1 (XOR -> Door Controller) con Splitter | la puerta no abre: FALLA | PASA |
| B1 con trazas: cambios de alimentación del Door Controller por paso (A on, B on, A off, B off) | 0/0/0/0, con 280-480 líneas `[LogicGate]` por paso | 1/1/1/1 en dos montajes, 2-10 líneas por paso |
| B2 | (no repetido hoy) | PASA |
| Q -> Toggle (activar A voltea B una vez; desactivarla no) | PASA | PASA (1/0/1/0) |
| Transiciones XOR 10 -> 00 y AND 11 -> 00 -> 10 con celda en Set | PASA | PASA |
| C1-C7 con Splitter (C1, C2, C3 toggle de 5 pulsos, C5, C6 tras reinicio, C7) | (no repetido hoy) | PASA |
| D5 con Splitter, coche cruzando el haz | la celda no se activa: FALLA | PASA |
| D5 con Power directo | (no repetido hoy) | PASA |
| D5 Laser -> interruptor -> Set | no concluyente (beam 0.0 en ese sitio) | PASA |
| Regresión sin Splitter: A, B, C (C2, C3, C5, C7) | (no repetido hoy) | PASA |
| D1-D4 | (no repetido hoy) | D1 detecta y suelta a los 3 s; D2 detecta el coche; D3 detecta el camión; D4 no detecta el coche vacío |
| Sobrecarga con XOR abierto y 9 lámparas | | 0 cambios de salida en 60 s, 0/9 lámparas, generador `overloaded=1`; con B on la puerta cierra en 11 y sale de la sobrecarga |
| Vecino ruidoso (40 Splitters, 81 lámparas, 122 cables; interruptor conmutado cada 500 ms) | OR 1xxx y XOR 1xx1 ya solos | OR y XOR siguen su tabla solos y con ruido, estables y sin cambios de más |
| G2: RPT al cierre ordenado | sin `SCRIPT (E)` salvo fugas; 1 fuga LFPG (`LFPG_BTCPriceCallback`) | igual |

LED de entrada en la sobrecarga: la operación `logic_state` del banco lee el flujo
(`IsPortReceivingPower`), no el LED. En el arreglo, `m_Input0Powered`/`m_Input1Powered` y el latch salen
de la misma lectura de señal (`LFPG_LogicGate.c:136-170`), y el latch dio XOR(1,0) = 1 con A on y B off.
El LED sigue a los interruptores por código; no se leyó directamente.

## Coste (§7)

Método: los dos PBO con trazas, los mismos pasos y el mismo banco de partida. Por evento, las líneas
`[ElecGraph] ProcessDirtyQueue: processed= edges= remaining=` (`LFPG_ElecGraphImpl.c:3015-3018`) desde
la marca del evento hasta `remaining=0`.

| Evento | BASE: llamadas / nodos / aristas | Arreglo: llamadas / nodos / aristas |
|---|---|---|
| Sin lógica: generador -> Splitter -> 3 lámparas, apagar o encender (x6) | 1 / 7 / 73 | 1 / 7 / 73 |
| Sin lógica: cortar un cable (x3) | 1 / 8 / 53 | 1 / 8 / 53 |
| Sin lógica: recablear (x3) | 2 / 12 / 96 | 2 / 12 / 96 |
| A: AND 01 | 1 / 7 / 85 | 1 / 7 / 89 |
| A: AND 10 | 5 / 73 / 840 | 5 / 72 / 868 |
| A: AND 11 | 124 sin asentar | 3 / 54 / 661 |
| C: Set, Reset, Set | 42, 43 y 150 sin asentar | 3, 2 y 3 |
| D5 con Splitter: entrar, salir, Reset | 47, 69 y 747 sin asentar | 2, 1 y 2 |
| Vecino: conmutación del árbol | unas 22 por conmutación, sin vaciar | 6 / 139 / unas 1374 |

Sin lógica: 0 % de diferencia. Con lógica: el arreglo asienta en 6 llamadas de PDQ como mucho (criterio:
10); BASE no asienta. La sonda `LFPG_PERF_PROBE` no sirve para estos eventos: solo abre un evento en las
rutas R01 y R04 (corte por RPC del jugador) y R02, y las operaciones del banco no pasan por ellas
(`events=0` con los dos PBO de sonda).

## «Requeue limit reached» (anexo de GL-02)

Con los mismos pasos y el mismo banco: **arreglo 154, BASE 7587**.

Corrección: el informe anterior daba 416 con el arreglo frente a 1 en BASE. Venía de sesiones con mundos
distintos (la sesión BASE cargaba los circuitos guardados por la del arreglo) y no vale como comparación.

Arreglo, por nodo: Memory Cell de C 46 (y 10 y 5 en otras celdas); Splitters 43, 9, 7, 6, 5 y 3;
pulsadores e interruptores entre 7 y 1; AND 1. Por caso: sin lógica 0 al encender o apagar y 1 por
recableado; AND 9 en 10 y 9 en 11; C 5, 5 y 8; D5 3, 3 y 5; vecino 1 por conmutación; el resto en la
secuencia C1-C7 con sus cortes.

Mecanismo, con dos secuencias PT_CHAIN del arreglo:

- AND de 01 a 10 (19:19:37 hora local): al abrir, la lámpara empieza a pedir 10 y la demanda sube por
  el Splitter a escalones (lastStable 2 -> 10,5 -> 15 -> 20; carga del generador 0,02 -> 0,21 -> 0,3 ->
  0,4). Mientras sube, el reparto deja a 0 las salidas de los interruptores (all-off). Todas las marcas
  son `mask=2` (DIRTY_INPUT): reencolados de Step 3 aguas abajo y demanda aguas arriba de PASSTHROUGH
  (`:2839-2870` de la base). El tope llega en la época 28 en el interruptor B, el Splitter y el
  interruptor A. La puerta sigue abierta por señal (in0, in1 y gate a true) aunque el flujo dé 0; el
  latch no parpadea.
- Memory Cell en Reset (19:22:00 hora local): `SetOutputPortEnabled output_0 false` (mask=5) cambia su
  demanda (lastStable 1 -> 11). Hay un ping-pong con el Splitter de arriba (3 -> 13) mientras la celda
  queda sin alimentación (inSum 0) y los dos llegan al tope en la época 257.

Es la rampa de demanda de la capa de energía (GL-02), que en BASE pasa igual y mucho más. No es la
señal parpadeando en nodos con autoconsumo, ni bits que nacen a false, ni un ping-pong disparado por
`m_SignalChanged`. En estado estable no hay ninguno con el arreglo.

## G1: reinicio limpio

Volcado del grafo (`LFPG_Diag.DumpGraphFull`) antes y después de un reinicio ordenado, con los mismos
pasos en las dos versiones: 159 -> 161 nodos y 158 -> 160 cables en las dos. No viene del arreglo.

- Vuelven tres cables cortados en C5 y C7 con la operación `unwire` del banco: `btn.output_1 ->
  cell.input_1`, `sw.output_1 -> cell.input_1` y `brst.output_1 -> cell.input_2` (comprobado con
  `wire_state` tras el reinicio, en las dos versiones). Causa, del arnés: `unwire` del driver gs01 quita el
  cable de `m_Wires` pero no llama a `LFPG_CommitWireMutation`, así que `m_WireGeneration` no sube y
  `LFPG_GetWiresJSON` (`LFPG_WireOwnerBase.c:190-198`) sigue sirviendo la caché vieja, que es la que
  guardan CE y el diario (`:114-140`). Vuelve el corte de un dueño sin ninguna mutación real después; el
  Splitter y el pulsador de C3 se recablearon por el camino normal, que regenera la caché. El corte real
  del jugador sí sube la generación (`HandleCutPort` -> `PublishOwnerCutDelta` ->
  `LFPG_CommitWireMutation`, `LFPG_RPCServerHandlerImpl.c:2051` y `:1931-1937`). Ya estaba anotado el
  26-10. La comprobación con el corte real por RPC queda para la prueba final de la 1.3.1.
- Falta tras el reinicio un horno (`LFPG_Furnace` 1067220793:1596636757) con su cable al Combiner
  1450028251:1897783867. Es un objeto del banco de una sesión anterior (creado con `spawn` del driver
  gs01, cableado por el driver). El servidor lo borra unos segundos después del volcado, antes del
  cierre, en las dos versiones: `[CutAll] All wires removed` sin `[DeviceLifecycle] Moved`, `Picked up`
  ni `OnDeviceKilled`, que es el camino de `OnDeviceDeleted` (EEDelete, `LFPG_DeviceLifecycle.c:56-66`).
  Tras el reinicio no existe. La causa del borrado no está demostrada; lo más probable es la limpieza
  de CE por tiempo de vida. No es un cable creado ni cortado en esta sesión. Neto: +3 -1 = +2 nodos y +2
  cables, igual en las dos versiones.
- C6 con el arreglo: la celda sigue activa los 60 s posteriores al reinicio.

## Lo que no va en partida

- L-RACE y la vuelta de una entidad (§8): se quedan en los tests (§6.4).
- El banco grande: no se corrió.

## Evidencia en el PC

- Logs por caso y tiempos: `C:\tmp\lfpg-grok\runs\n131\evidence\f3fix-*`, `f3base-*`, `f3fixdiag-*`,
  `f3basediag-*`, `f3fixperf-*`, `dfix*`, `dbase*`.
- Script logs de las sesiones con trazas: `evidence\dfix-script.log`, `dfix2-script.log`,
  `dbase-script.log`, `dbase2-script.log`.
- Volcados del grafo: `C:\tmp\lfpg-grok\runs\n131\dump-before.txt`, `dump-after.txt`,
  `dumpb-before.txt`, `dumpb-after.txt`.
- Drivers y análisis: `fase3.py`, `f3run.sh`, `evcost.py`, `dumpdiff.py`, `wirecheck.py`, `b1count.py`.
- Copias del banco y restos de las pruebas: `C:\tmp\lfpg-grok\bench-bak-f3`, `bench-bak-f3b`.
