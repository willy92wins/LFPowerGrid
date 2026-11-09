# BRIEF-GRAFO-SPLITTER — puertas logicas y Laser -> Set detras de un Splitter (1.3.1)

Estado: APROBADO. Guillermo eligio el 2026-11-09 11:07Z arreglarlo dentro de la 1.3.1 ("hay que
arreglar el grafo, es prio"). Base: `main` 592ebcc (#97 fusionada). Rama
`claude/project-thread-0iolln` reiniciada desde `origin/main`; PR nuevo en borrador. Primero
diagnostico y plan a ciegas, despues implementacion. Lee `AGENTS.md` entero; si algo de aqui lo
contradice, manda `AGENTS.md` y dilo en el informe.

## Sintomas medidos (ronda 1 en partida, DayZDiag, BASE 1326FCC3 y NUEVO b5834ff; repetidos el 11-09)

Mediciones completas en `reviews/2026-11-08-nostra-131/GRAFO-SPLITTER-MEDICIONES.md`.

1. AND/OR/XOR con las dos entradas alimentadas desde salidas de un mismo Splitter (cada una por su
   interruptor) oscilan y la lampara no enciende; A4 (XOR, salida sin cable, A encendido) da 73
   cambios de estado en 60 s. Igual en BASE y en NUEVO. Con cada interruptor en una salida directa
   del generador, las tres tablas pasan. El XOR de dos interruptores a un Door Controller (B1) falla
   igual con Splitter y pasa sin el.
2. Memory Cell con Power (`input_0`) desde el Splitter y Set (`input_3`) desde un Laser Detector que
   cuelga del mismo generador (D5, y tambien con Power por otra linea): el laser abre, pero Set nunca
   se ve alimentado. El grafo da al laser una oferta de 5 - 5 = 0 hacia Set y toda la demanda de la
   celda sale por Power (`scripts/5_Mission/LFPG_ElecGraphImpl.c` ~4015
   `ComputeOfferBasePassthrough`, ~4022 `PassthroughSupplyAt`).
3. Lo que ya pasa detras del Splitter y no puede romperse: Memory Cell con pulsadores en Set, Reset y
   Toggle (C1-C7, con el filtro de 400 ms por entrada que ya esta en `main`).

## Hipotesis (a confirmar o tumbar en el diagnostico)

- Los dispositivos logicos leen sus entradas por flujo asignado: `IsPortReceivingPower` (~5013-5044)
  mira `edge.m_AllocatedPower > LFPG_PROPAGATION_EPSILON` por puerto.
- La demanda hacia un PASSTHROUGH es su `m_LastStableOutput` (~4602) y, si el nodo tiene varios
  proveedores, `ApplyMergerWaterFill` (~4390) reparte esa demanda entre ellos. Es correcto para
  repartir energia, pero puede dejar a 0 una entrada que si tiene tension. Una entrada logica con
  tension y sin flujo se lee apagada.
- La sonda `LFPG_GATE_PROBE_DEMAND` (1.0) solo se aplica con la puerta cerrada (~2665, ~4642). Al
  abrirse, la demanda pasa al reparto, una entrada se queda sin flujo, la puerta se cierra, vuelve
  la sonda, se abre... de ahi la oscilacion.
- Sin Splitter las entradas vienen de proveedores distintos y el reparto no deja ninguna a 0, por
  eso pasa.
- Contexto, no objetivo: el umbral de `IsPortReceivingPower` y el de `m_AllocChanged` (~4793) no
  casan (TA3-01, en el bug-ledger). Si el arreglo toca la lectura de entradas, dilo; no lo arregles
  de paso si no hace falta.

## Direccion candidata (no es una decision: cada plan a ciegas puede proponer otra)

Que cada entrada de senal de un dispositivo logico (las dos de AND/OR/XOR; Toggle, Reset y Set de
la Memory Cell; no Power) pida siempre al menos la sonda, abierta o cerrada, ademas de su parte
del reparto. Una entrada con tension llevaria entonces flujo >= sonda y se leeria bien. Coste: hasta
1 u/s por entrada de senal con tension. Alternativa a evaluar: leer las entradas logicas por oferta
disponible en vez de por flujo asignado.

Riesgos que el plan tiene que tratar: la asignacion binaria (un distribuidor sobrecargado pone a 0
todas sus salidas una epoca, B5) si las sondas suben la demanda total cerca del limite; regresion
en dispositivos no logicos (Splitter, Combiner, baterias, consumidores): tienen que dar exactamente
lo mismo; convergencia (requeue en la misma epoca, sin oscilar); coste por tick; persistencia y
sincronizacion (nada de lo que se guarda o se envia al cliente cambia de formato).

## Fases

1. **Diagnostico y plan a ciegas** (R22, solo lectura), dos familias en paralelo: Grok y Codex
   escriben cada uno su plan sin ver el otro ni ningun plan previo. Cada plan trae:
   - la causa confirmada en el codigo (`path:line`) de los sintomas 1 y 2, o por que la hipotesis cae;
   - una reproduccion offline: un test que da rojo con el fuente de hoy (o con el oraculo
     `graph_reference` montando el circuito de A3 y de D5) y que pasaria con el arreglo;
   - el arreglo minimo, con cada linea que cambia trazada a la causa (R20, R25);
   - tests con fixtures positivas y negativas, codigos de salida y mutantes que muerden (R26);
   - el tratamiento de cada riesgo de arriba, y lo que no se toca.
   Salidas: `reviews/2026-11-09-grafo-splitter/PLAN-grok.md` y `PLAN-codex.md`. Reconciliacion en el
   hilo antes de tocar codigo; el plan consolidado pasa por R22 de los dos.
2. **Implementacion** (un escritor, Grok), gates, tests offline que muerdan y revision R21 de Codex.
3. **Partida** con el PBO del arreglo frente a BASE = `main` 592ebcc: A1-A4 y B1-B2 con Splitter;
   C1-C7 y D5 (Laser -> Set) con Splitter; regresion sin Splitter (A, B, C, D1-D4); G1-G2; el banco
   grande si se puede. Al acabar, el banco como estaba.

## Criterios de aceptacion

- Offline: el test de reproduccion da rojo sobre el fuente de hoy (o sobre un mutante que lo
  represente) y verde con el arreglo; todos los `test_graph_*.py` y el oraculo `graph_reference` en
  verde; ningun cambio de reparto en los fixtures existentes salvo los que el plan justifique uno
  por uno; `test_graph_perf_probe.py` sin empeorar.
- En partida: A1-A3 siguen su tabla detras de un Splitter en menos de 3 s; A4 con 0 o 1 cambios en
  60 s; B1 invierte la puerta en cada cambio; D5 abre y se queda abierto, y Reset la cierra; C1-C7
  siguen pasando; ningun `SCRIPT (E)` nuevo; como mucho 1 `Leaked 'LFPG_` al cerrar.
