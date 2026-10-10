# RONDA 2 — lane i78 (medición)

Revisión de `b66dee8`. Las sondas están bien colocadas: todas detrás de `if (LFPG_PERF_PROBE)` y
solo añaden líneas. Hay dos bloqueantes: uno rompe el cliente y otro pone el CI en rojo.

## M1 (BLOQUEANTE) — Compilación del cliente
`LFPG_PerfProbe.c` envuelve la clase en `#ifdef SERVER`. Pero `LFPG_RPCServerHandlerImpl.c` se
compila también en el cliente: la clase no está bajo `#ifdef SERVER` (mira sus líneas 21-26, que
declaran miembros `#ifndef SERVER`).

Las llamadas de R01 y R04 quedan fuera de cualquier `#ifdef SERVER`: `:982`, `:993`, `:1114`,
`:1131`, `:1147-1148`, `:2084-2090`, `:2122-2123` y `:2152`. En el cliente `LFPG_PerfProbe` no
existe, el módulo 5_Mission no compila y ningún jugador podría entrar. El
`if (LFPG_PERF_PROBE)` no lo evita: Enforce compila la llamada aunque la condición sea false. Ni
el linter ni `enforce_checks` lo detectan.

Arreglo: quita el `#ifdef SERVER` de `LFPG_PerfProbe.c`. La clase es inerte en el cliente: con la
constante en false nadie la llama, y las rutas son de servidor.

Test nuevo: calcula el contexto de preprocesador (`#ifdef`, `#ifndef`, `#else`, `#endif`) de la
clase y de cada llamada a `LFPG_PerfProbe.`, y falla si alguna llamada se compila en un build donde
la clase no existe. Negativo: con la clase envuelta otra vez en `#ifdef SERVER`, el test falla. La
versión `b66dee8` tiene que fallar.

## M2 (BLOQUEANTE) — CI en rojo
`test_negative_head_base_lacks_probes` hace `git show HEAD:` y exige que HEAD no tenga sondas.
Pasó en tu ronda porque HEAD era la base. Una vez commiteado, HEAD es tu commit y falla.
Reproducido en `b66dee8`:

```
FAIL: test_negative_head_base_lacks_probes
AssertionError: True is not false
```

Quítalo. Los negativos por mutación (`test_negative_missing_each_route`,
`test_negative_probe_enabled_is_rejected`) ya cubren lo que pide el brief.

## M3 — `g_Game` en 3_Game
El repo no usa `g_Game` en 3_Game: `LFPG_Telemetry.c:128` lo evita a propósito, y en 3_Game se usa
`GetGame()` (`LFPG_BTCConfig.c:457`). Usa `GetGame().GetTime()` en `LFPG_PerfProbe.c`, o cita con
`path:line` dónde se declara `g_Game` en el 3_Game vanilla.

## M4 — Un `Begin` anidado borra el evento de fuera
R01 y R04 llaman a `PostBulkRebuildAndPropagate`, que llama a `LFPG_DeviceAPI.SetPowered` y a
`m_Graph.PostBulkRebuild`. Si algo de esa cadena llega a `RequestPropagate` →
`RefreshSourceState` → `MarkUpstreamNodesDirty`, pasa esto:

1. `Begin("R02")` reinicia los contadores;
2. su `End()` apaga el evento;
3. las `Phase`/`End` de R01 se pierden sin línea.

Elige una de dos:
- Demuestra con la cadena de llamadas citada que no se alcanza.
- O haz que un `Begin` con un evento activo no reinicie el de fuera:
  - el interior suma sus visitas, requeues y allocs al exterior;
  - sus `Phase` y `End` no tocan las fases del exterior;
  - la línea del exterior lleva `nested=N`.

Test: comprobación por texto de la guarda en `Begin`, `Phase` y `End`, con su negativo por
mutación.

## M5 — Inventario R-05 sin `path:line`
El brief pide cada bucle con `path:line`, y la tabla de `MEDICION.md` da nombres de función.
Añade `path:line` a cada fila.

Además, `index_calls` es una cota estática calculada antes del bucle (`Count()` o `Count() * 2`),
no una cuenta medida. Dilo en `MEDICION.md`. En cada fila instrumentada pon la fórmula y por qué:
cuántos `GetKey`/`GetElement` hace cada iteración.

## M6 — `allocs` y activación (texto)
La línea de R01 lleva `allocs=2` (+3 si hay rescue). Las allocations de la reconstrucción
(`PostBulkRebuildAndPropagate`, `RebuildFromWires`...) no se cuentan.

- Que `MEDICION.md` diga exactamente qué cuenta `allocs` y qué no.
- Que el umbral de R-01 no se apoye en ese número.
- En "Como activar": hace falta un build propio con la constante en true, en un servidor de
  pruebas, con cliente y servidor cargando el mismo PBO.

## Criterio de hecho
El de la ronda 1, más M1 y M2 con sus tests:
- el test de M1 falla contra `b66dee8` y pasa ahora;
- ningún test depende de lo que haya en HEAD. La sesión que commitea los vuelve a correr con el
  commit ya hecho.

Lista blanca: la de la ronda 1. Añade al final de `INFORME.md` una sección `## Ronda 2` con lo que
cambió. Mismo RECEIPT.
