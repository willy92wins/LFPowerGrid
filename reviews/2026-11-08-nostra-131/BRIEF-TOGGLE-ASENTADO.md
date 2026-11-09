# BRIEF-TOGGLE-ASENTADO v3 — Toggle de la Memory Cell sobre lecturas asentadas (1.3.1)

v3 del 2026-11-09. Corrige la v2 tras `REVIEW-codex-plan-toggle-v2.md` (NO-APTO): TA2-01, un
temporizador que llega tarde deja que una lectura nueva sustituya un cambio que ya habia cumplido
400 ms; TA2-02, el arnes no ejecuta la carga ni el borrado. Cambios frente a la v2: se aplica lo vencido
antes de cualquier lectura nueva, el temporizador ya no relee los puertos, el borrado va en
`LFPG_OnDeleted`, y los tests de TA2-01 y TA2-02. Sigue sustituyendo los parches de rondas 6-8. Rama
`claude/project-thread-0iolln`. Lee `AGENTS.md`; si algo de aqui lo contradice, manda este brief y dilo
en el informe.

## Por que

La celda solo ve niveles por puerto. Detras de un Splitter, cada cambio del grafo produce apagones de
convergencia de 73-100 ms en todas sus salidas (medido en la ronda 1). Cada regla sobre lecturas crudas
cierra un orden y abre otro (R6-01, R6-02, R7-01, R7-02, R7-03). Una suelta real durante un apagon y un
interruptor fijo cuya rama tarda en volver pueden dar el mismo historial, asi que el contrato se acota
con un filtro de asentamiento **por entrada**.

## Contrato

1. Un cambio de Power o de Toggle solo existe si esa entrada lo mantiene al menos
   `LFPG_MCELL_DEBOUNCE_MS` (400 ms; exactamente 400 cuenta). Lo mas corto se ignora en los dos
   sentidos. Cada entrada se filtra por separado: un parpadeo de una no retrasa ni anula el cambio de la
   otra (TA-01).
2. El instante de cada cambio es el de su propia ultima lectura cruda distinta (R7-03). La celda solo lee
   sus puertos en `LFPG_SetPowered`, que el grafo llama cuando la asignacion de una entrada cambia en mas
   de `LFPG_PROPAGATION_EPSILON` (`SyncNodeToEntity` no se salta con `LFPG_DIRTY_INPUT`). Un cruce del
   umbral de `IsPortReceivingPower` con una variacion menor que ese epsilon no se notifica
   (`LFPG_ElecGraphImpl.c:4793` frente a `:5044`, TA3-01): ese cambio se ve en la siguiente llamada, con
   el instante de esa lectura. Solo pasa con asignaciones de milesimas de u/s; la celda consume 1 u/s.
3. Los cambios asentados se aplican **en orden de esos instantes**, Power antes que Toggle solo si
   empatan (TA-02, F12), y **siempre antes de que una lectura nueva de esa entrada los sustituya**
   (TA2-01). Un temporizador que llega tarde cambia cuando se ve la inversion, nunca cuantas hay.
4. Toggle invierte en un flanco de subida asentado si, en el instante del flanco, (R1) Toggle llevaba
   asentado bajo al menos `LFPG_MCELL_TOGGLE_REARM_MS` (1000 ms), (R2) Power llevaba asentado alto al
   menos esos 1000 ms, (R3) paso la gracia tras cargar y (R4) Toggle se vio bajo alguna vez.
5. Consecuencias aceptadas: un corte de Power de 400 ms o mas es un apagon de verdad y una subida de
   Toggle en el segundo siguiente a su vuelta no cuenta; dos pulsaciones a menos de 1 s no cuentan; una
   rama de Toggle sin energia 1 s o mas con Power presente cuenta como soltar, y su vuelta como pulsar
   (solo pasa con un grafo que oscila: el fallo de Splitter pendiente, fuera de esto).
6. Set y Reset no cambian: por nivel, despues del flanco (R5).

## Cambios en `scripts/4_World/LFPG_MemoryCell.c` (nada fuera de este fichero y sus tests)

- `static const int LFPG_MCELL_DEBOUNCE_MS = 400;` junto a `LFPG_MCELL_TOGGLE_REARM_MS`.
- Fuera: `LFPG_MCellPowerRestartCounter`, `m_PowerLowSinceMs`, `m_ToggleLowInOutage`, `m_ToggleLastHigh`
  y su codigo en `LFPG_SetPowered`. Se queda `LFPG_MCellToggleShouldFlip` tal cual.
- Miembros de sesion (no se persisten): `bool m_RawPower = false; bool m_RawToggle = true;
  int m_RawPowerChangedMs = -1; int m_RawToggleChangedMs = -1; bool m_SettledPower = false;
  bool m_SettledToggle = true; bool m_FlipPending = false;` y los que ya estan `m_PowerSinceMs`,
  `m_ToggleLowSinceMs`, `m_ToggleGraceUntilMs` (ahora de lecturas asentadas). `m_RawToggle` y
  `m_SettledToggle` empiezan en true para R4.
- Funcion pura, firma en una linea:
  `static bool LFPG_MCellSettleDue(bool raw, bool settled, int changedMs, int nowMs, int debounceMs)`:
  true si `raw != settled` y `changedMs >= 0` y `nowMs - changedMs >= debounceMs`.
- `protected void LFPG_MCellApplyDue(int nowMs)` (cuerpo en `#ifdef SERVER`), lo que en la v2 eran los
  pasos 2 y 3 de `LFPG_MCellSettle`:
  1. `dueP = LFPG_MCellSettleDue(m_RawPower, m_SettledPower, m_RawPowerChangedMs, nowMs, LFPG_MCELL_DEBOUNCE_MS)`
     y `dueT` igual con Toggle.
  2. Aplica los vencidos en orden de `m_Raw*ChangedMs`; si vencen los dos y los instantes empatan, Power
     primero.
     - Power: `m_SettledPower = m_RawPower;` y `m_PowerSinceMs = m_RawPowerChangedMs` si sube, -1 si baja.
     - Toggle: al subir, si `LFPG_MCellToggleShouldFlip(m_ToggleLowSinceMs, m_PowerSinceMs,
       m_ToggleGraceUntilMs, m_RawToggleChangedMs, LFPG_MCELL_TOGGLE_REARM_MS)`, `m_FlipPending = true;`;
       al bajar, `m_ToggleLowSinceMs = m_RawToggleChangedMs`. Luego `m_SettledToggle = m_RawToggle`.
  Cada entrada tiene como mucho un candidato, asi que una llamada aplica como mucho un flanco de Toggle.
  Si vence el de Toggle, el de Power con instante anterior tambien ha vencido (mismo plazo), asi que el
  orden se conserva entre llamadas.
- `LFPG_SetPowered`, tras leer los cuatro puertos:
  1. `LFPG_MCellApplyDue(nowMs);` **antes de tocar ningun `m_Raw*`** (TA2-01).
  2. Si `hasPower != m_RawPower`: `m_RawPower = hasPower; m_RawPowerChangedMs = nowMs;`,
     `Remove(LFPG_MCellPowerSettleTimer)` y `CallLater(LFPG_MCellPowerSettleTimer, LFPG_MCELL_DEBOUNCE_MS, false)`.
  3. Si `hasToggle != m_RawToggle`: lo mismo con `m_RawToggle`, `m_RawToggleChangedMs` y
     `LFPG_MCellToggleSettleTimer`. Cada entrada solo toca su temporizador.
  4. Si `m_FlipPending`: `m_LatchState = !m_LatchState; m_FlipPending = false;`.
  5. Despues, Set/Reset por nivel, estado, enrutado y diferido como hoy, sin tocar.
- `void LFPG_MCellPowerSettleTimer()` y `void LFPG_MCellToggleSettleTimer()`: los dos solo llaman a
  `LFPG_MCellSettle()`.
- `void LFPG_MCellSettle()` (cuerpo en `#ifdef SERVER`): `LFPG_MCellApplyDue(nowMs);` y, si
  `m_FlipPending`, `LFPG_SetPowered(m_PoweredNet);`, para que el flanco pase por el mismo camino de
  Set/Reset, enrutado y sincronizacion. **Sin relectura de puertos** (la v2 la tenia; por el punto 2 del
  contrato no hace falta, y sus variantes de la revision pasan a ser lecturas en un `LFPG_SetPowered`:
  F19 y F20).
- `override void LFPG_OnDeleted()` como el de `LFPG_PushButton.c` (`if (g_Game)` y `Remove` en
  `CALL_CATEGORY_SYSTEM`), con los dos temporizadores. La celda hoy no quita `LFPG_DeferredRouting` al
  borrar: dejalo asi y anotalo en el informe para el bug-ledger.
- La carga (R3) no cambia.
- Comentario R1-R5 de `LFPG_SetPowered` reescrito con el contrato de arriba, corto.

`LFPG_PushButton` y `LFPG_SwitchRemote` se quedan como en 24c4c48 (el pulso no se cancela por un corte):
sin eso, un pulso cortado por un apagon duraria menos de 400 ms y el filtro lo tiraria.

## Tests (R26), con fuente real y que muerdan

1. `test_memorycell_toggle.py`, reescrito. Conserva los casos de `LFPG_MCellToggleShouldFlip` y anade
   los de `LFPG_MCellSettleDue` (P, N y limite `nowMs - changedMs == debounceMs`). Arnes con trozos de
   `LFPG_SetPowered`, `LFPG_MCellApplyDue`, `LFPG_MCellSettle`, los dos envoltorios,
   `LFPG_OnStoreLoadDevice` y `LFPG_OnDeleted` cargados del fuente, y una cola simulada en Python a la
   que llegan los `CallLater` y `Remove` del propio fuente (un temporizador por entrada, cancelar el suyo
   anterior, disparar a los 400 ms mas el retraso que pida la fixture). El arnes ejecuta las cuatro lecturas de `IsPortReceivingPower` del fuente contra un NetworkManager simulado que contesta por puerto, sin inyectar `hasPower` ni `hasToggle`; leer Power del puerto de Toggle (o al reves) tiene que romper los tests (TA3-02). Las constantes y los valores
   iniciales de los miembros se leen del fuente, no se copian (TA-03). Fixtures `(t,P,T)`, latch inicial
   OFF y gracia vencida salvo F15; resultado = numero de inversiones, latch final y, donde se indica, el
   instante de la inversion. Con temporizadores puntuales y, en F18, en los dos ordenes entre lectura y
   temporizador del mismo milisegundo:
   - F1 pulsacion con apagon: `(0,1,0) (10000,1,1) (10010,0,0) (10083,1,1) (12000,1,0)`: 1, en 10483.
   - F2 cinco pulsaciones de 2 s separadas 3 s, con un apagon de 73 ms al pulsar y al soltar: 5. En
     detalle: inicio `(0,1,0)`; para `a = 10000 + 5000k`, `k = 0..4`: `(a,1,1) (a+10,0,0) (a+83,1,1)
     (a+2000,1,0) (a+2010,0,0) (a+2083,1,0)`.
   - F3 C5, interruptor fijo y corte de 10 s: `(0,1,1) (20000,0,0)` y luego `(30000,0,1) (30050,1,1)`,
     `(30000,1,0) (30050,1,1)` o `(30000,1,1)`: 0 en las tres.
   - F4 R6-01 `(0,1,1) (5000,0,0) (7500,0,1) (8000,1,1)`: 0.
   - F5 R6-02 `(0,1,1) (5000,0,0) (5600,1,0) (5700,0,0) (6300,1,1)`: 0.
   - F6 R7-01 `(0,1,1) (5000,0,0) (5600,1,0) (5700,0,0) (6600,1,1)`: 0.
   - F7 R7-02 `(0,1,1) (5000,1,0) (5050,0,0) (5950,1,0) (6000,1,1)`: 0.
   - F8 R7-03 `(0,1,0) (1500,1,1) (3500,0,0) (3600,1,0) (4550,1,1) (6000,1,1)`: 2, en 1900 y 4950.
   - F9 pulsacion dentro de un apagon `(0,1,0) (5000,0,0) (5050,0,0) (5100,1,1)`: 1.
   - F10 Set alto desde el inicio; Toggle sube en 5000 y baja en 7000; Reset bajo: sigue en ON.
   - F11 doble pulsacion `(0,1,0) (5000,1,1) (7000,1,0) (7600,1,1) (9600,1,0)`: 1.
   - F12 Power cae mientras Toggle sube `(0,1,0) (5000,0,1) (8000,0,0)`: 0.
   - F13 TA-01 `(0,1,0) (5000,1,1) (5300,0,1) (5600,1,1) (5800,1,0)`: 1, en 5400.
   - F14 TA-02 `(0,1,0) (5000,1,1) (5100,0,1) (6000,0,1)`: 1.
   - F15 gracia, que pone la carga ejecutada (`LFPG_OnStoreLoadDevice` con reloj en 0, no se inyecta):
     `(0,1,0) (5000,1,1) (7000,1,0)`: 0; con la carga en 0 y `(0,1,0) (31000,1,1) (33000,1,0)`: 1.
   - F16 alto de 800 ms `(0,1,0) (5000,1,1) (5800,1,0)`: 1.
   - F17 dos subidas validas `(0,1,0) (5000,1,1) (5600,1,0) (7000,1,1) (7600,1,0)`: 2.
   - F18 limite exacto `(0,1,0) (5000,1,1) (5400,1,0)`: 1 en los dos ordenes.
   - F19 Power bajo exactamente 400 ms en la subida `(0,1,0) (5000,0,1) (5400,1,1)`: 0.
   - F20 Toggle alto 500 ms y Power cae despues `(0,1,0) (5000,1,1) (5100,0,1) (5500,0,0)`: 1.
   Con temporizadores tarde (mismo resultado que puntual): F13 y F16 con el de Toggle 401 ms tarde; F17
   con el de Toggle 3000 ms tarde; F7 con el de Power 1000 ms tarde; F14 con el de Toggle 200 ms tarde.
   Y una prueba con semilla fija: al menos 1000 secuencias aleatorias de 3 a 10 lecturas (huecos de 10,
   50, 73, 100, 300, 399, 400, 401, 600, 999, 1000, 1001, 1400, 2000 y 5000 ms) con retrasos aleatorios
   de 0 a 3000 ms por temporizador, contra un oraculo independiente por tramos (cada entrada se asienta
   si mantiene 400 ms, los cambios asentados se recorren por instante con Power primero en el empate, y
   R1-R4): el arnes con el fuente real tiene que coincidir en todas.
   - Borrado: con los dos temporizadores pendientes, `LFPG_OnDeleted` ejecutado; avanzar la cola no
     despacha ninguno y no queda ninguno.
   - Mutantes, cada uno rompe al menos lo indicado: filtro a 0 ms (F8, F9); un solo instante de cambio
     para las dos entradas (F8); un solo temporizador compartido (F13 por el instante 5400); Power siempre
     primero, sin mirar instantes (F14 con retraso); Toggle primero en el empate (F12, F19); sin R1
     (F11); sin R2 (F3-F7, F12, F19); rearme de produccion a 0 (F11); sin gracia (F15); la asignacion de
     la gracia en la carga cambiada por `m_ToggleGraceUntilMs = 0;` (F15); sin `LFPG_MCellApplyDue` en
     `LFPG_SetPowered` (F13, F16 y F17 tarde, F7 con Power tarde); sin cualquiera de los dos `Remove` de
     `LFPG_OnDeleted` (borrado).
   - Fixtures, oraculo, retrasos y mutantes comprobados con un modelo en Python de este contrato antes de
     escribir el brief: las fixtures dan lo esperado puntuales, tarde y en los dos ordenes; 3000
     secuencias aleatorias con retrasos aleatorios coinciden con el oraculo; cada mutante de la lista
     rompe lo suyo. El test tiene que dar lo mismo con el fuente real.
2. `test_pulse_survives_blackout.py` (R7-04), como en la v2: pulsar, cortar Power, vencer el plazo y
   borrar, con una cola controlada, en `LFPG_PushButton` y `LFPG_SwitchRemote`, ejecutando el fuente;
   comprueba `m_SwitchOn` y los callbacks pendientes en cada paso. Mutantes: guarda de `LFPG_PulseOff`
   invertida, plazo a 0 y sin limpieza al borrar; los tres fallan (hoy pasan los tres, segun la
   revision).

## Gates y entrega

Los de `BRIEF.md` (linter delta 0, convenciones, EOL, todos los `test_*.py` en verde, diff minimo).
Antes de implementar: R22 corta de Codex acotada a lo que cambia frente a la v2 (el vaciado, la
relectura quitada, el borrado y los tests), con el contrato como marco. Salida en
`reviews/2026-11-08-nostra-131/REVIEW-codex-plan-toggle-v3.md`. Commit "(Grok, ronda N)", push, Codex R21
del commit. En partida, con el PBO nuevo y el montaje del plan (detras del Splitter): C1, C2, C3 (cinco
alternancias), C5 tres veces, C6 y C7.
