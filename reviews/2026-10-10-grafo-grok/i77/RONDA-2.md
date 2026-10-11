# RONDA 2 — lane i77 (reloj de cargadores)

Revisión de `02e8526`. La forma del cambio en `scripts/` es correcta: modded class con `super`
primero, solo SERVER, `GetExisting()` y el reloj publicado antes de `AddEnergy`. Los tests nuevos
pasan. Pero hay un defecto real en G03-b, y varias pruebas no prueban lo que dicen.

## C1 (defecto, G03-b) — El hook depende del orden del motor
El hook solo cierra G03-b si el motor ya ha vaciado el slot cuando llama a `EEItemDetached`. Si
`EEItemDetached` corre con la batería todavía en el slot:

1. `UpdateVanillaChargerPower` ve la misma batería y cierra el intervalo hasta el detach;
2. guarda `lastBattery` = esa batería;
3. al reinsertarla, el hook de attach ve `sameBattery` verdadero y acredita todo el tiempo que
   estuvo fuera.

Reproductor con tu `Clock` del test, que carga el `UpdateVanillaChargerPower` real:

- `visit(1, True)`.
- Detach en t=2 con la batería aún en el slot: `update`; después, slot = None.
- Reinserción en t=40: slot = la misma batería; `update`.
- `visit(63, True)`.
- Resultado: 62.0 u. Lo esperado es 1 + 23 = 24.
- Con el slot ya vacío al llamar al detach da 23.0, que es correcto.

Offline no se puede saber en qué orden lo hace el motor, así que el arreglo tiene que funcionar en
los dos órdenes: en el detach, después de cerrar el intervalo, olvida la batería guardada. Por
ejemplo:

- `NotifyVanillaChargerAttachment(EntityAI charger, bool detached)`;
- si `detached`, `m_ChargerBatteries.Remove(nodeId)` tras `UpdateVanillaChargerPower`.

Así el detach acredita el tramo hasta el instante del evento si la batería sigue en el slot, y la
reinserción siempre rebasa el reloj.

Si encuentras evidencia citable del orden, cítala en el informe; el arreglo va igual. Puedes leer,
solo lectura, los scripts vanilla en `P:\scripts` si existe, y el Knowledge Pack en
`C:\Users\guill\DayZ-Modding-Knowledge-Pack`.

## C2 (pruebas de G03-b) — De texto a comportamiento
`test_g03b_poll_only_same_object_reinsert_still_credits_the_gap` y
`test_g03b_negative_missing_notify_leaves_gap_credit_path_unclosed` solo comprueban texto. El
negativo comprueba que tu propio `re.subn` funcionó, no que algún test detecte la falta del hook.

Sustitúyelos por una prueba de comportamiento: la secuencia detach → attach → poll, pasando por el
`UpdateVanillaChargerPower` real del `Clock`, en los dos órdenes del motor (slot vacío o lleno al
llamar a `EEItemDetached`).

El método del hook tiene un bucle y el slicer no lo carga. Elige una de dos y dilo en el informe:
- saca la parte posterior al match a un método escalar que el test pueda cargar;
- o comprueba por texto que el método real contiene exactamente las sentencias que el test
  simula.

En los dos casos, el test comprueba por texto que `EEItemAttached` pasa `false` y `EEItemDetached`
pasa `true`.

Resultados esperados:
- orden "slot vacío": 23 u;
- orden "slot lleno": 24 u.

Negativos:
- sin hook: 62 u;
- con el hook de `02e8526`, en orden "slot lleno": 62 u.

Deja en GATES la salida del test nuevo contra `02e8526` (tiene que fallar) y con tu cambio (pasa).

## C3 (finales de línea)
`scripts/4_World/LFPG_ElecGraph.c` quedó con todas sus líneas en CRLF: el diff es +64/−60 y el
cambio real son +4. Lo repara la sesión que commitea. Tú no reformatees nada y toca solo las
líneas que cambies.

## C4 (menor) — Pruebas que no prueban lo que dicen
- `test_g02_energy_per_fed_time_is_independent_of_node_count_and_dirty_queue`: la variable
  `nodes` no se usa. La independencia del número de nodos la prueban las aserciones
  estructurales (el tick no toca `m_Nodes`, `m_DirtyQueue` ni `ProcessDirtyQueue`). Que el nombre
  y el comentario digan eso, y quita el bucle falso de nodos.
- `test_g02_tick_visits_and_latency_bound_from_production_constants` y la segunda mitad de
  `test_g03a_switch_poll_error_bound_from_constants`: los bucles comparan una fórmula consigo
  misma. Deja la lectura de las constantes de producción y la cota calculada una vez.

## C5 (menor) — Informe y protocolo
- G03-c y G02: no cambiaste nada, así que el veredicto es INALCANZABLE (el defecto no se puede
  producir), con su argumento y su test. No ARREGLADO.
- G03-a: "un hook no cubre todos los caminos vanilla (SetSwitchedOn scripted, EM interno)" no
  está verificado. Cítalo con `path:line`, del vanilla o del Knowledge Pack. Por ejemplo: si
  `OnSwitchOn`/`OnSwitchOff` de `EntityAI` se llaman en todo `SwitchOn`/`SwitchOff` del
  `ComponentEnergyManager`. Si no puedes citarlo, pásalo a `LO QUE NO PUDE VERIFICAR`. La
  recomendación (aceptar la cota: con C ≤ 32, ≤ 1 u) puede quedarse; la decisión es del dueño.
- G03-d: actualiza la cota con C1. Con el slot lleno al detach, la pérdida es ~0. Con el slot
  vacío, hasta un intervalo de visita.
- `PROTOCOLO-INGAME.md`, G03-b: añade la lectura que distingue los dos órdenes (la energía de la
  batería justo al sacarla).

## Criterio de hecho
El de la ronda 1, más C1 con el test de C2 fallando contra `02e8526` y pasando ahora. Lista
blanca: la de la ronda 1. Añade al final de `INFORME.md` una sección `## Ronda 2` con lo que
cambió. Mismo RECEIPT.
