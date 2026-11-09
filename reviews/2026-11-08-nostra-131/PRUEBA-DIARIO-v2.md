# Prueba en partida — diario de cables (v2)

v2 del 2026-11-09, para `BRIEF-DIARIO.md` v2. DayZDiag en el PC de Guillermo, con el lease de DayZ MCP
(`20_Runbooks/dayz-mcp-agent-session-protocol.md`), mundo `lfpg_grafo`. Nunca el servidor de los
jugadores.

- **CONTROL** = build sin diario: el PBO de la ronda 1 (o el ultimo sin diario que tengas). Los casos
  **(neg)** tienen que perder los cambios aqui, o la prueba no demuestra nada.
- **DIARIO** = PBO del commit del diario, compilado aislado (linter 0 errores, `build_isolated_pbo`).
- Al terminar: `@LFPowerGrid_grafo` otra vez con 1326FCC3, `storage_1` y `LF_PowerGrid.json` como
  estaban, y fuera los `wire_journal.json*` que hayan creado las pruebas.

**Pasadas de la CE.** Vigila las mtimes de `storage_1/data/dynamic_*.bin` para saber cuando empieza y
acaba cada pasada. Con `LFPG_LOG_LEVEL` en Debug, `first-emit`, `durable`, `recapture` y `saved` dicen
lo que hace el diario.

**Muerte sucia entre pasadas** (como `INFORME-2.md`): justo al acabar una pasada, haz el cambio, espera al
menos 10 s, copia `wire_journal.json` como evidencia y `taskkill /F` antes de que empiece la siguiente.
**Muerte a mitad de pasada**: haz el cambio entre pasadas, espera a que empiece la siguiente (primera
mtime nueva, o `first-emit <id>` del aparato en el log) y `taskkill /F` antes de que acabe. Si el momento
no es el pedido, el caso no vale: repitelo.

Evidencia por caso: pasa/falla, lineas `[WireJournal]` del RPT o script log, consulta de cables antes y
despues, y la copia del fichero.

- **J1 (neg)** Entre aparatos ya guardados: un cable nuevo, un corte de un cable guardado y un reroute de
  otro. Muerte entre pasadas. CONTROL: se pierden los tres. DIARIO: siguen los tres; `Replay ...
  restored=` >= 1 con sus lineas `Restored <id>`, sin `SCRIPT (E)` nuevos.
- **J2 (neg)** Sustitucion entre duenos: S1 -> T:in guardado; cablea S2 -> T:in. Muerte entre pasadas.
  CONTROL: vuelve S1 -> T:in. DIARIO: S2 -> T:in y S1 sin ese cable, `restored=2`.
- **J3** Doble caida: tras el arranque de J1 con DIARIO, en cuanto salga `Startup validation done` y
  antes de la primera pasada, mata otra vez. Tercer arranque: los cambios siguen.
- **J4** Reinicio limpio tras cambios: cables bien y `restored=0`.
- **J5** Retirada: un cambio y espera a que pasen al menos tres pasadas con el servidor en marcha. Pasa
  si sale `durable <id>` y la copia del fichero, **cargada y contada** (no por el log), tiene 0
  entradas. Mata entonces: el arranque sale con `entries=0` y los cables intactos.
- **J6** Estado inesperado: tras una muerte como J1, con el servidor parado, cambia `m_SavedJSON` y
  `m_SavedPrevJSON` de una entrada por `{}`. Arranca DIARIO. Pasa si sale `Skipped <id>` y ese aparato
  queda como lo cargo la CE, mientras las demas entradas se restauran.
- **J7** Desactivado: `"WireJournal": false`, reinicio y J1. Pasa si se comporta como CONTROL, el RPT
  dice `WireJournal=0` (o `false`) y `wire_journal.json` desaparece al arrancar y no vuelve.
- **J8a** Principal corrupto con `.bak` valido: con el servidor parado, corta `wire_journal.json` por la
  mitad y deja el `.bak`. Pasa si arranca sin `SCRIPT (E)`, con un `Warn [WireJournal]` de que usa el
  `.bak`, y el replay usa sus entradas.
- **J8b** Principal corrupto sin `.bak`: igual, borrando tambien el `.bak`. Pasa si arranca sin
  `SCRIPT (E)`, con `Warn`, diario vacio, los cables de la CE, y tras un cambio nuevo el fichero vuelve a
  ser valido.
- **J9** Aparato posterior al guardado: coloca un aparato justo despues de una pasada y cablea a el uno
  guardado; muerte entre pasadas. Pasa si el aparato nuevo no esta, el cable hacia el se poda, y no hay
  `SCRIPT (E)`.
- **J10** Coste (observacion): 30 cableados en 60 s con Debug. Anota cuantas lineas `saved` salen
  (deberian ser <= 13), el `ms=` maximo, el tamano maximo del fichero y el maximo de entradas.
- **J11 (WJ-01)** Muerte a mitad de pasada: un cambio entre pasadas y muerte a mitad de la siguiente,
  con `first-emit <id>` del aparato ya en el log. CONTROL (informativo): anota si la CE cargo el estado
  viejo o el nuevo. DIARIO: el cambio sigue, con `Restored <id>` o `unchanged` para ese aparato. Hazlo
  tres veces; pasa si las tres conservan el cambio y en ninguna sale `Skipped`.
- **J12 (WJ-02)** Tras el arranque de un J11 con `Restored`, sin cambios nuevos, muerte a mitad de la
  primera pasada. Pasa si el cambio sigue en el tercer arranque.

Lo que el banco de pruebas no pueda hacer, marcalo NO EJECUTADO con el motivo; no lo des por bueno.
