# Prueba en partida — diario de cables (ronda 2)

DayZDiag en el PC de Guillermo, con el lease de DayZ MCP (`20_Runbooks/dayz-mcp-agent-session-protocol.md`),
mundo `lfpg_grafo`. Nunca el servidor de los jugadores. Va despues de los casos A-G de
`PRUEBA-EN-PARTIDA.md`, en la misma sesion si se puede.

- **CONTROL** = build sin diario: el PBO de la ronda 1 (S1-S4) si lo compilaste para A-G; si no,
  BASE 1326FCC3. Los casos marcados **(neg)** tienen que perder los cambios aqui, o la prueba no
  demuestra nada.
- **DIARIO** = PBO del commit de la ronda 2, compilado aislado (linter 0 errores, `build_isolated_pbo`).
- Al terminar: `@LFPowerGrid_grafo` otra vez con 1326FCC3, `storage_1` y `LF_PowerGrid.json` como
  estaban, y fuera los `wire_journal.json*` que hayan creado las pruebas.

**Muerte sucia** (procedimiento de `C:\tmp\lfpg-cables-reinicio\INFORME-2.md`): vigila las mtimes de
`storage_1/data/dynamic_*.bin`; justo al acabar un ciclo de guardado, haz el cambio, espera al menos
10 s (el diario escribe cada 5 s), copia `wire_journal.json` como evidencia y `taskkill /F` antes de
que empiece el ciclo siguiente. Si se cuela un ciclo, el caso no vale: repitelo.

Evidencia por caso: pasa/falla, lineas del RPT o script log (`[WireJournal]`, con `LFPG_LOG_LEVEL`
en Debug para `ack`, `drop` y `saved`), consulta de cables antes y despues, y la copia del fichero.

- **J1 (neg)** Entre aparatos ya guardados: un cable nuevo, un corte de un cable guardado y un
  reroute de otro. Muerte sucia. CONTROL: se pierden los tres. DIARIO: siguen los tres; el RPT dice
  `[WireJournal] Replay ... restored=` >= 1 con sus lineas `Restored <id>`, sin `SCRIPT (E)` nuevos.
- **J2 (neg)** Sustitucion entre duenos: S1 -> T:in guardado; cablea S2 -> T:in (lo sustituye).
  Muerte sucia. CONTROL: vuelve S1 -> T:in y falta el de S2. DIARIO: S2 -> T:in y S1 sin ese cable,
  `restored=2`.
- **J3** Doble caida: tras el arranque de J1 con DIARIO, en cuanto salga
  `Startup validation done` y antes del primer ciclo de la CE, mata otra vez. Tercer arranque: los
  cambios siguen.
- **J4** Reinicio limpio tras cambios: cables bien y `restored=0`.
- **J5** Limpieza: un cambio y al menos 6 min con el servidor en marcha. Pasa si el fichero queda con
  0 entradas (o el Debug muestra `drop ... ` por ack). Mata entonces: el arranque sale con
  `entries=0` o `restored=0` y los cables intactos.
- **J6** Proteccion contra estados inesperados: tras una muerte sucia como J1, con el servidor
  parado, cambia el `m_BaseJSON` de una entrada por otro texto (por ejemplo `{}`). Arranca DIARIO.
  Pasa si sale `Skipped <id>` y ese aparato se queda como lo dejo la CE, mientras las demas entradas
  se restauran.
- **J7** Desactivado: `"WireJournal": false` en `$profile:LF_PowerGrid\LF_PowerGrid.json`, reinicio y
  J1. Pasa si se comporta como CONTROL, el RPT dice `WireJournal=0` (o `false`) y
  `wire_journal.json` desaparece al arrancar y no vuelve. Deja el JSON como estaba.
- **J8** Fichero corrupto: con el servidor parado, corta `wire_journal.json` por la mitad. Pasa si
  arranca sin `SCRIPT (E)`, con un aviso `[WireJournal]` o de `[FileUtil]`, los cables de la CE, y
  si tras un cambio nuevo el fichero vuelve a ser valido.
- **J9** Aparato posterior al guardado: coloca un aparato justo despues de un ciclo y cablea a el uno
  guardado; muerte sucia. Pasa si el aparato nuevo no esta (DayZ), el cable hacia el se poda al
  arrancar, y no hay `SCRIPT (E)`.
- **J10** Coste (observacion): 30 cableados en 60 s con Debug. Anota cuantas lineas
  `[WireJournal] saved` salen (deberian ser <= 13) y el `ms=` maximo; cualquier tiron visible del
  servidor se anota.

Lo que el banco de pruebas no pueda hacer, marcalo NO EJECUTADO con el motivo; no lo des por bueno.
