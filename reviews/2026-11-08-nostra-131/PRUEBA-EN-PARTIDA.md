# Prueba en partida — arreglos del reporte de Nostra

DayZDiag en el PC de Guillermo, con el lease de DayZ MCP (`20_Runbooks/dayz-mcp-agent-session-protocol.md`).
Nunca el servidor de los jugadores. Dos builds:

- **BASE** = el PBO de prueba de la 1.3.1 ya desplegado en `P:\Mods\@LFPowerGrid_grafo\Addons`
  (SHA-256 `1326FCC3...`, build de b225206 = PR #96: main 9a4bafd + version 1.3.1 + arreglo del foco). Sirve de control negativo: los casos marcados
  **(neg)** tienen que FALLAR aqui, si no la prueba no demuestra nada.
- **NUEVO** = PBO de la rama `claude/project-thread-0iolln` (commit que diga el informe), compilado
  aislado como la 1.3.1 (linter 0 errores, `build_isolated_pbo`). Al terminar, deja
  `@LFPowerGrid_grafo` con el PBO BASE y el storage de la mision como estaba.

De dia (los paneles solo dan entre 6:00 y 20:00), o con generador. Si el driver de pruebas no puede
hacer un paso (por ejemplo meter a un jugador en un vehiculo), marcalo NO EJECUTADO con el motivo;
no lo des por bueno.

Evidencia por caso: pasa/falla, lineas del RPT o del script log (con `LFPG_LOG_LEVEL` en Debug si
hace falta: `[MemoryCell] SetPowered`, `[LogicGate]`, `[LFPG_DoorController]`, `[FinishWiring-Server]`)
y una captura cuando el resultado se ve (LEDs, puerta, chat).

## A. Puertas logicas (codigo sin cambios; en BASE y en NUEVO)

Montaje: generador -> Splitter; Splitter salida 1 -> interruptor A -> puerta entrada 0; salida 2 ->
interruptor B -> puerta entrada 1; puerta salida -> lampara (consumidor).

- A1 AND, A2 OR, A3 XOR: las cuatro combinaciones de A/B. Pasa si la lampara sigue la tabla
  (AND 0001, OR 0111, XOR 0110) en menos de 3 s tras cada cambio y los LEDs de entrada coinciden.
- A4: en XOR, deja la salida sin cable y A encendido 60 s. Anota si los LEDs parpadean o si el log
  muestra cambios de estado repetidos (observacion, no criterio).

## B. Una puerta con dos interruptores via XOR (BASE y NUEVO)

Montaje A3, pero la salida del XOR -> Door Controller emparejado con una puerta de valla (o la de
un edificio a menos de 2 m).
- B1: partiendo de A y B apagados (puerta cerrada), haz A on, B on, A off, B off. Pasa si cada
  cambio invierte la puerta (abre, cierra, abre, cierra) en menos de 5 s.
- B2: con la puerta abierta, reinicio limpio del servidor. Pasa si vuelve abierta y no se mueve sola.

## C. Memory Cell

Montaje C: generador -> Splitter; salida 1 -> Power (`input_0`); salida 2 -> pulsador -> Set;
salida 3 -> pulsador -> Reset. Output (`output_0`) -> Door Controller (o lampara). **Inverted sin cable.**

- C1 **(neg)** Set con la celda en OFF. BASE: no cambia (bug S2a). NUEVO: pasa a ON y la puerta abre.
- C2 NUEVO: Reset -> OFF; Set -> ON; Reset -> OFF. Pasa si cada pulsacion da el estado esperado.
- C3 NUEVO: cambia el pulsador de Set a Toggle (`input_1`). Cinco pulsaciones separadas >= 3 s.
  Pasa si el estado alterna exactamente cinco veces (ON, OFF, ON, OFF, ON), sin rebote. Esto incluye
  el apagon de convergencia: la salida 3 del mismo Splitter alimenta el pulsador.
- C4 **(neg)** BASE: el mismo C3 no alterna (la salida sigue al pulso y vuelve).
- C5 NUEVO: con la celda en ON, cambia el pulsador de Toggle por un interruptor fijo encendido en
  Toggle. Apaga el generador 10 s y enciendelo. Pasa si la celda sigue en ON tras volver.
- C6 NUEVO: celda en ON e interruptor fijo encendido en Toggle; reinicio limpio. Pasa si vuelve en ON
  y no cambia en los 60 s siguientes.
- C7 NUEVO: Set mantenido (interruptor fijo en Set) y un pulso en Toggle. Pasa si sigue en ON.

## D. Laser Detector

Montaje D: generador -> Laser IN; Laser OUT -> lampara. Haz horizontal a ~0,6 m del suelo.
- D1 (BASE y NUEVO): un jugador a pie cruza. Pasa si la lampara se enciende y se apaga ~3 s despues.
- D2 **(neg)** coche con conductor cruza el haz despacio. BASE: no se detecta o se detecta a ratos.
  NUEVO: la lampara se enciende mientras alguna parte del coche corta el haz y se apaga ~3 s despues.
- D3 NUEVO: camion (V3S/M3S) con conductor; el haz corta la parte de atras con la cabina a mas de 6 m
  del laser. Pasa si se detecta.
- D4 NUEVO: coche vacio aparcado en el haz. Pasa si NO se detecta.
- D5 NUEVO (escenario de Nostra): Laser OUT -> Set de una Memory Cell montada como C (Reset con
  pulsador, Output -> Door Controller). Sal con el coche por el haz: la puerta abre y se queda
  abierta; el pulsador de Reset la cierra.

## E. Aviso de cable sustituido (NUEVO)

- E1: cablea generador -> lampara 1 y luego la misma salida del generador -> lampara 2. Pasa si el
  chat dice `Replaced 1 existing cable: each port holds only one cable.` y el cable a la lampara 1
  ya no esta.
- E2: con interruptor A -> entrada del Door Controller, cablea interruptor B -> esa entrada. Mismo aviso.
- E3 (sin aviso): vuelve a tender el mismo cable entre los mismos dos puertos por otro camino
  (reroute). Pasa si NO hay aviso.
- E4 (sin aviso): cable a un puerto libre. Pasa si NO hay aviso.
- E5: `"NotifyWireReplaced": false` en `$profile:LF_PowerGrid\LF_PowerGrid.json` del servidor de
  pruebas, reinicio, repite E1. Pasa si NO hay aviso y el RPT dice `NotifyWireReplaced=0` (o `false`).
  Deja el JSON como estaba.

## F. Textos (NUEVO)

- F1: el inspector o la descripcion del kit de AND, OR, XOR, PressurePad y Laser dice 100 u/s; la
  Memory Cell dice 100 u/s y 1 u/s de autoconsumo.

## Regresion minima (NUEVO)

- G1: reinicio limpio con todos los montajes puestos: vuelven todos los cables y estados.
- G2: RPT sin `SCRIPT (E)` nuevos de LFPG respecto a BASE; como mucho 1 `Leaked 'LFPG_` al cerrar.
