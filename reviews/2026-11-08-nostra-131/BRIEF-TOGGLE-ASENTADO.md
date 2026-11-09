# BRIEF-TOGGLE-ASENTADO v2 — Toggle de la Memory Cell sobre lecturas asentadas (1.3.1)

v2 del 2026-11-09. Corrige la v1 tras `REVIEW-codex-plan-toggle.md` (NO-APTO por TA-01 y TA-02, las dos
de compartir un temporizador y un orden fijo; TA-03 del arnes). Sustituye los parches de rondas 6-8
(histeresis de Power, `m_PowerLowSinceMs`, `m_ToggleLowInOutage`, `LFPG_MCellPowerRestartCounter`), que
Codex declaro UNSOUND dos veces (`REVIEW-codex-r3-pulsos.md`, `REVIEW-codex-r4-toggle.md`). Rama
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
   `LFPG_MCELL_DEBOUNCE_MS` (400 ms). Lo mas corto se ignora en los dos sentidos. Cada entrada se filtra
   por separado: un parpadeo de una no retrasa ni anula el cambio de la otra (TA-01).
2. El instante de cada cambio es el de su propia ultima lectura cruda distinta (R7-03).
3. Los cambios asentados se aplican **en orden de esos instantes**, Power antes que Toggle solo si
   empatan (TA-02, F12).
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
- Funcion pura nueva, firma en una linea:
  `static bool LFPG_MCellSettleDue(bool raw, bool settled, int changedMs, int nowMs, int debounceMs)`:
  true si `raw != settled` y `changedMs >= 0` y `nowMs - changedMs >= debounceMs`.
- `LFPG_SetPowered`, tras leer los cuatro puertos:
  - si `hasPower != m_RawPower`: `m_RawPower = hasPower; m_RawPowerChangedMs = nowMs;`,
    `Remove(LFPG_MCellPowerSettleTimer)` y `CallLater(LFPG_MCellPowerSettleTimer, LFPG_MCELL_DEBOUNCE_MS, false)`;
  - si `hasToggle != m_RawToggle`: lo mismo con `m_RawToggle`, `m_RawToggleChangedMs` y
    `LFPG_MCellToggleSettleTimer`. Cada entrada solo toca su temporizador;
  - si `m_FlipPending`: `m_LatchState = !m_LatchState; m_FlipPending = false;`;
  - despues, Set/Reset por nivel, estado, enrutado y diferido como hoy, sin tocar.
- `void LFPG_MCellPowerSettleTimer()` y `void LFPG_MCellToggleSettleTimer()`: los dos solo llaman a
  `LFPG_MCellSettle()`.
- `void LFPG_MCellSettle()` (cuerpo en `#ifdef SERVER`):
  1. Relee Power y Toggle con `IsPortReceivingPower`; si alguno difiere de su `m_Raw*`, actualizalo con
     la hora actual y reprograma **solo su** temporizador, como en `LFPG_SetPowered`.
  2. `dueP = LFPG_MCellSettleDue(m_RawPower, m_SettledPower, m_RawPowerChangedMs, nowMs, DEBOUNCE)` y
     `dueT` igual con Toggle.
  3. Aplica los que esten vencidos en orden de `m_Raw*ChangedMs`; si los dos vencen y los instantes
     empatan, Power primero.
     - Power: `m_SettledPower = m_RawPower;` y `m_PowerSinceMs = m_RawPowerChangedMs` si sube, -1 si baja.
     - Toggle: al subir, `flip = LFPG_MCellToggleShouldFlip(m_ToggleLowSinceMs, m_PowerSinceMs,
       m_ToggleGraceUntilMs, m_RawToggleChangedMs, LFPG_MCELL_TOGGLE_REARM_MS)`; al bajar,
       `m_ToggleLowSinceMs = m_RawToggleChangedMs`. Luego `m_SettledToggle = m_RawToggle`.
  4. Si `flip`: `m_FlipPending = true;` y `LFPG_SetPowered(m_PoweredNet);`, para que el flanco pase por
     el mismo camino de Set/Reset, enrutado y sincronizacion.
  Como cada temporizador procesa todo lo vencido en orden, un temporizador que llegue tarde no cambia el
  resultado (F13).
- La carga (R3) no cambia. En el borrado, quita tambien los dos temporizadores de la cola donde hoy se
  quita `LFPG_DeferredRouting` (si hoy no se quita nada al borrar, anotalo y quita solo los dos nuevos).
- Comentario R1-R5 de `LFPG_SetPowered` reescrito con el contrato de arriba, corto.

`LFPG_PushButton` y `LFPG_SwitchRemote` se quedan como en 24c4c48 (el pulso no se cancela por un corte):
sin eso, un pulso cortado por un apagon duraria menos de 400 ms y el filtro lo tiraria.

## Tests (R26), con fuente real y que muerdan

1. `test_memorycell_toggle.py`, reescrito. Conserva los casos de `LFPG_MCellToggleShouldFlip` y anade
   los de `LFPG_MCellSettleDue` (P, N y limite `nowMs - changedMs == debounceMs`). Arnes con trozos de
   `LFPG_SetPowered` y de `LFPG_MCellSettle` cargados del fuente, y una cola simulada en Python con **un
   temporizador por entrada** (programar al cambiar esa entrada, cancelar el suyo anterior, disparar a
   los 400 ms). Las constantes (`LFPG_MCELL_DEBOUNCE_MS`, `LFPG_MCELL_TOGGLE_REARM_MS`, la gracia) y los
   valores iniciales de los miembros se leen del fuente, no se copian (TA-03). Fixtures `(t,P,T)`, latch
   inicial OFF y gracia vencida salvo F14; resultado = numero de inversiones y latch final:
   - F1 pulsacion con apagon: `(0,1,0) (10000,1,1) (10010,0,0) (10083,1,1) (12000,1,0)`: 1.
   - F2 cinco pulsaciones de 2 s separadas 3 s, con un apagon de 73 ms al pulsar y al soltar: 5.
   - F3 C5, interruptor fijo y corte de 10 s: `(0,1,1) (20000,0,0)` y luego `(30000,0,1) (30050,1,1)`,
     `(30000,1,0) (30050,1,1)` o `(30000,1,1)`: 0 en las tres.
   - F4 R6-01 `(0,1,1) (5000,0,0) (7500,0,1) (8000,1,1)`: 0.
   - F5 R6-02 `(0,1,1) (5000,0,0) (5600,1,0) (5700,0,0) (6300,1,1)`: 0.
   - F6 R7-01 `(0,1,1) (5000,0,0) (5600,1,0) (5700,0,0) (6600,1,1)`: 0.
   - F7 R7-02 `(0,1,1) (5000,1,0) (5050,0,0) (5950,1,0) (6000,1,1)`: 0.
   - F8 R7-03 `(0,1,0) (1500,1,1) (3500,0,0) (3600,1,0) (4550,1,1) (6000,1,1)`: 2.
   - F9 pulsacion dentro de un apagon `(0,1,0) (5000,0,0) (5050,0,0) (5100,1,1)`: 1.
   - F10 Set mantenido y un pulso en Toggle (C7): sigue en ON.
   - F11 doble pulsacion `(0,1,0) (5000,1,1) (7000,1,0) (7600,1,1) (9600,1,0)`: 1.
   - F12 Power cae mientras Toggle sube `(0,1,0) (5000,0,1) (8000,0,0)`: 0.
   - F13 TA-01 `(0,1,0) (5000,1,1) (5300,0,1) (5600,1,1) (5800,1,0)`: 1.
   - F14 TA-02 `(0,1,0) (5000,1,1) (5100,0,1) (6000,0,1)`: 1; y otra vez con el temporizador de Toggle
     entregado 200 ms tarde: 1.
   - F15 gracia: `(0,1,0) (5000,1,1) (7000,1,0)` con la gracia hasta 30000: 0; `(0,1,0) (31000,1,1)
     (33000,1,0)` con la misma gracia: 1.
   - Mutantes, cada uno rompe al menos la fixture indicada: filtro a 0 ms (F8, F9); un solo instante de
     cambio para las dos entradas (F8); un solo temporizador compartido (F13); Power siempre primero, sin
     mirar instantes (F14 con retraso); Toggle primero en el empate (F12); sin R1 (F11); sin R2 (F3-F7,
     F12); rearme de produccion a 0 (F11); sin gracia (F15).
   - Fixtures y mutantes comprobados con un modelo en Python del contrato antes de escribir este brief:
     todas dan lo esperado, tambien con el temporizador de Toggle 200 ms tarde, y cada mutante rompe la
     suya. El test tiene que dar lo mismo con el fuente real.
2. `test_pulse_survives_blackout.py` (R7-04): pulsar, cortar Power, vencer el plazo y borrar, con una cola
   controlada, en `LFPG_PushButton` y `LFPG_SwitchRemote`; comprueba `m_SwitchOn` y los callbacks
   pendientes en cada paso. Mutantes: guarda de `LFPG_PulseOff` invertida, plazo a 0 y sin limpieza al
   borrar; los tres fallan.

## Gates y entrega

Los de `BRIEF.md` (linter delta 0, convenciones, EOL, todos los `test_*.py` en verde, diff minimo).
Antes de implementar: R22 de Codex de esta v2 contra la rama, con el contrato como marco (un historial
que el contrato declara indistinguible no es hallazgo; uno que lo viola, si). Salida en
`reviews/2026-11-08-nostra-131/REVIEW-codex-plan-toggle-v2.md`. Commit "(Grok, ronda N)", push, Codex R21
del commit. En partida, con el PBO nuevo y el montaje del plan (detras del Splitter): C1, C2, C3 (cinco
alternancias), C5 tres veces, C6 y C7.
