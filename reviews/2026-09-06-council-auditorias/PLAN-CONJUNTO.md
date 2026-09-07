# Plan conjunto de acción — LFPowerGrid
### Arbitraje de un council de 3 lanes sobre dos auditorías · 2026-09-07

**Corrida:** brief `sha256:4157d7e6…`, 73.832 B, byte-idéntico en las tres lanes.
Lanes: `cursor-grok-4.6-xhigh`, `kimi-k3-max`, `glm-5.2-max` — las tres verdes, modelo servido ==
pin. Codex fuera: 100 % de ventana semanal. Árbitro: el orquestador (Anthropic), que no es lane.

---

## 0. Salud de la corrida, antes de leer nada más

**No es estrictamente `COMPARABLE`, así que no sirve para rankear proveedores.** El manifiesto
declaró `puede_leer_arbol: no` para las tres, con workspace aislado. A la vuelta resulta que
**kimi y grok sí leyeron el árbol** — kimi cita `HANDOFF.md:155-161` y `config.cpp:1080,1086`, que
no estaban en el brief — mientras **glm no** («confío en las citas del brief»). Tres lanes con
información distinta no juegan al mismo juego. Se degrada a `COBERTURA`: los hallazgos únicos
valen, la comparación entre lanes no. Causa: `--workspace` de `cursor-agent` no confina la
herramienta de lectura. Corregir en la próxima corrida.

---

## 1. Las tres rechazaron la pregunta, por unanimidad

La pregunta era «¿se jubila la V3 antes o después de los P1 de integridad?». Las tres, sin verse:
**es un falso binario**. Empaqueta 25 fichas P1 heterogéneas como un bloque, cuando solo dos de
ellas (S04, S08) comparten subsistema con la jubilación.

**Y el borrador del orquestador tenía cinco errores concretos, todos verificados después:**

| # | Error | Quién | Verificación |
|---|---|---|---|
| 1 | «Las dos auditorías dan órdenes incompatibles» | grok | **Falso.** La auditoría muse nunca ordena jubilar la V3: su Top 5 es congelar el set MCP, no tocar `McpJsonEscape`, decidir la paridad, gatear `WriteMcpDump` y hoist/dedup. Quien pone la consolidación primero es **la rama en vuelo y mi propio borrador**. El conflicto real es *rama viva vs §8 de la auditoría grande* |
| 2 | «T0 y T1 tocan el mismo subsistema, hacerlos aparte es tocar el ATM dos veces» | las 3 | **Falso. Solape = cero ficheros.** T0a toca `LFPG_BTCAtmView.c`/`Controller.c`; T1 toca `LFPG_BTCHelper.c`, `LFPG_BalanceProvider_NativeImpl.c`, `LFPG_FileUtil.c`, `LFPG_BTCAtm.c`. Es acoplamiento de producto, no de líneas. **Toda la razón de ser de mi orden T0→T1 se cae aquí** |
| 3 | «24 fichas del sorter habría que escribirlas dos veces» | grok, kimi | Inflado y **circular**: mi propia tabla solo metía S04 y S08 en los tramos. La lógica es compartida (`RPCServerHandlerImpl.c:142-150`: los dos subIds de sort caen al mismo handler). El impuesto real es UI/cliente y doble verificación |
| 4 | «Ninguno de los 22 commits atacó integridad» | grok | **Falso.** `ef29b73` (26-ago) arregló parcialmente SEC09, y está en las cuatro ramas |
| 5 | Omitía **R15 y D16**, 2 de las 25 P1 | grok, kimi | Confirmado: programaba 23 de 25 |

Corrección de cifra propia: escribí **106** referencias a la V3 desde fuera de ella; el recuento
real es **113**. Grok detectó el descuadre desde mis propios sumandos.

**Crítica al instrumento, de kimi, que acepto:** «fichero intacto ⇒ ficha viva» solo vale si la
cura tendría que tocar ese fichero. Para fichas del tipo «falta un llamador» (G04, G18) la cura
vive en **otro** fichero, y el citado quedaría intacto con la ficha ya muerta. Las 6 P1 y las 28
P2/P3 marcadas «vivas por construcción» heredan ese sesgo. Se cierra barato con una lectura
dirigida de las de ese tipo.

---

## 2. La corrección que manda sobre todo: la huella no es el premio

| Cifra | Valor | Fuente |
|---|---|---|
| Hueco de arena disponible | **885 kB** (el `355 kB` está **[RETRACTADO]**) | `assumptions.md:15`, 2026-08-23, vía `HANDOFF.md:155-158` |
| Lo que rinde jubilar la V3 | **~21 kB** = 7 clases × 3,0 kB/clase | proyección sobre la ratio A2 medida |
| Menos la clase nueva de paleta | **~18 kB neto** | kimi |
| **En porcentaje del hueco** | **~2 %** | 18/885 |

Y el aviso más afilado del council, de grok: **la V4 es más grande que la V3** — 10 clases y
178.279 B frente a 7 clases y 143.010 B. Los ~21 kB son «quitar 7», no «volver a 7». Si el hook
MCP, el `Update()` polling y las clases `_TEST` extra se quedan, **el objetivo de arena puede
salir neutro o negativo**. Nadie había convertido «jubilar V3» en un **presupuesto de clases netas
del sorter**, que es la única forma de que la palanca signifique algo.

**Consecuencia:** la razón para jubilar la V3 que sobrevive al escrutinio **no es la huella**. Es
que el fork está vivo y divergiendo solo (ficha S10: las dos UIs ya difieren en protección de
preview), mientras los P1 están estables. *Se consolida primero lo que se degrada solo; se arregla
después lo que no se mueve.*

**Si el objetivo real sigue siendo arena**, GLM y kimi coinciden por separado en que falta el
trabajo que **ninguna auditoría hizo porque ninguna conocía el objetivo**: un censo de clases
muertas. Candidatas ya en las propias fichas — SEC18 (Migrators, infraestructura huérfana), E13
(compound actions sin consumidor), R22 (código y estado muertos), D04 (la clase PAS que se compila
y no está en `CfgVehicles`). Probablemente mejor kB/hora que todo el tramo de jubilación.

---

## 3. El plan

Atención ≠ dependencia: el DAG permite paralelo; un implementador único serializa.

### Decisiones del dueño, tomadas 2026-09-07 — y lo que cambian

| Decisión | Respuesta | Efecto sobre el plan |
|---|---|---|
| ¿Servidor con jugadores? | **Sí, servidor privado** | **Invierte el orden.** La pérdida de dinero deja de ser latente y acumula daño por sesión. T1 pasa a ser lo primero, y con ella sube toda la superficie explotable con jugadores reales |
| ¿Manda la huella? | **No: manda mantenibilidad** | La V3 se jubila igual, pero por matar el fork antes de que siga divergiendo solo. **No se promete ganancia de arena** y la huella deja de ser criterio de orden. T6 queda fuera |
| ¿Hook MCP al jubilar? | **Se queda, gateado** | Se conserva bajo `#ifdef` de diagnóstico o gate de configuración, de forma que no entre en el build publicado. Entra como paso propio de T0b |

**La consecuencia que ninguna auditoría vio, porque ninguna sabía que había jugadores:** con
servidor vivo, dos fichas dejan de ser deuda y pasan a ser superficie de ataque — **SEC02** (un
jugador salta la política `AllowCutOthersWires` reemplazando un cable) y **SEC20** (cliente
modificado inventa nombres de puerto y diverge store contra grafo).

**D16 — RIESGO ACEPTADO por el dueño, 2026-09-07.** El hallazgo es real en el código: cero
coincidencias de `owner|territor|authoriz|permission|codelock` en la ruta Fence de
`LFPG_DoorController.c:574-612`, y abre con `f.OpenFence()` directo. **Control compensatorio:** el
servidor ya carga mods que impiden construir y colocar objetos en bases ajenas, así que el
controlador no puede llegar a la valla de otro. El radio refuerza la aceptación:
`LFPG_DC_PAIR_DIST_SQ_FENCE = 4.0` es distancia **al cuadrado** (`LFPG_DoorController.c:24`), o
sea **2 metros** de emparejamiento. Riesgo residual, para que la aceptación conste con su borde:
una valla ajena a menos de 2 m de suelo donde el jugador sí puede construir — límite directo entre
dos bases. **Dependencia externa:** si algún día se retira el mod de protección de bases, D16
vuelve a ser explotable y hay que reabrirla. Sin gate in-game.

### P0 — Día cero, antes de tocar código
1. **Decisión de producto:** dirección de la divergencia S10 — qué protección de preview gana.
   *No se asume que la V4 sea la buena.*
2. ~~**Inventario de divergencias V3↔V4.**~~ **HECHO 2026-09-07** →
   [`INVENTARIO-DIVERGENCIAS-V3-V4.md`](INVENTARIO-DIVERGENCIAS-V3-V4.md). **14 divergencias: 5
   favorecen a V3, 5 a V4, 4 no decidibles.** Confirmó que borrar la V3 sin propagar dos cosas es
   una regresión silenciosa:
   - **D-01 (alto)** — la acción V3 exige `LFPG_IsPowered()` + `LFPG_IsLinked()` y `GetType()`
     exacto; la V4 no exige ninguno y usa `IsKindOf`. Ya se sabía; ahora está localizado.
   - **D-02 (alto, NUEVO — no lo tenía ninguna auditoría)** — **`LFPG_UIScaler` existe en la V3 y
     está ausente por completo en la V4**: `Capture` en `Init`, `ComputeScale`+`Apply` en `DoOpen`,
     `Reset` en `Cleanup`, más el escalado de tags y preview rows. Sin eso, el panel V4 se ve
     desproporcionado en cualquier resolución o DPI que no sea 1080p al 100 %. Fue deliberado para
     probar (el commit `7e86ca8` dice «mecanica sin UIScaler»), pero **convierte la V4 en no apta
     para producción tal cual**.
   - **D-04 y D-05 (medio)** — el hook MCP y la sonda `S1_PROBE` se colarían en producción. Y
     **`S1_PROBE` no está detrás de `LFPG_PERFDIAG_ENABLED`: dispara siempre** y escupe ~16 líneas
     al RPT en cada apertura del panel. Refuerza la decisión de gatear el hook MCP.
   - A favor de la V4: `CanEdit() = paired && powered` con re-poll cada 0,5 s (D-03), y las
     confirmaciones de borrado (D-07/08/09).
   - Se pierde con la V3: la pista «FAILED - REOPEN» (D-10), y los colores por tipo de regla
     quedan permutados (D-12) — un usuario acostumbrado ve categoría en azul donde antes era verde.
3. Paridad `powered/linked` de `LFPG_ActionOpenSorterPanel_TEST`: con la V4 pasando a producción,
   **se alinea con producción**. La nota `allowUnpowered` del handoff ya prohíbe que ese camino
   llegue a un build publicado; no hay decisión que tomar.

*P0 no bloquea a T1: son ficheros disjuntos. T1 arranca ya.*

### T0a — Extracción pura de paleta · seguro pase lo que pase, pero ya no es lo primero
- Mover `LFPG_ColorData` (`LFPG_SorterView.c:45`) y las 29 `COL_*` a un módulo neutro.
- Repuntar **solo el ATM**: `LFPG_BTCAtmView.c` (64 `COL_*` + 5 `LFPG_ColorData`) y
  `LFPG_BTCAtmController.c` (12). La V3 consume el módulo y no se rompe todavía.
- **NO entra:** unificar `LFPG_ColorData` con `LFPG_ColorData_TEST`. Eso toca `SetUserData`, donde
  la auditoría muse documenta un crash de heap (`0xc0000374`). No es extracción mecánica.
- Fin: `grep LFPG_SorterView.COL_` fuera de la V3 = 0; compila; el ATM pinta igual.

### T1 — Integridad monetaria · **PRIMERO** (hay jugadores) · no comparte un solo fichero con T0
- E04 (`LFPG_BTCHelper.c:1465`, `:1505-1513`), E16 (`BalanceProvider_NativeImpl.c:1230-1243` +
  `LFPG_BTCAtm.c:165-176`), SEC09 residual (`LFPG_FileUtil.c:214-218`, `:280-283`), E02, E03, E08.
- **Añadido por el council: E05**, que la auditoría grande agrupa en el mismo invariante de
  commit/abort.
- Invariante a nombrar antes de tocar nada: *ningún camino de error monetario deja RAM, disco e
  hive incoherentes.*
- **Cuidado con E15:** la reconciliación de arranque asume el orden commit/abort actual.
  Reordenar Sell/Deposit sin tocarla puede convertir un bloqueo temporal en pérdida.
- Fin: matriz de fallos **con inyección** y reinicio entre cada par de E/S. Sin harness no se
  declara (A03).

### T0b — Cerrar V4 y jubilar la V3 · después de T0a y de P0
1. Aplicar las decisiones de P0 (paridad, S10).
2. Emisor de `SORTER_TEST_RESYNC (65)` y productor de `SORTER_TEST_CARGO_REFRESH (70)`. El
   servidor ya los atiende con handlers compartidos; falta el lado cliente.
3. `LFPG_SorterView_TEST.IsOpen()` en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`.
4. Retirar los 9 enganches de `LFPG_MissionInit.c` y los 4 handlers de `LFPG_RPCClientHandler.c`;
   quitar el mutex anti dual-open (`LFPG_SorterView_TEST.c:1329`).
5. **Decidir el destino del hook MCP** (`LFPG_MCP_SorterCmd.layout`, `Update()` polling,
   `WriteMcpDump`). Dejarlo «porque es TEST» se come los ~21 kB. *No renombrar ese layout* —
   guarda explícita de la auditoría muse.
6. Borrar las 7 clases V3 y sus 3 layouts.
- **Alcance:** «sin refactor **no relacionado**». Cablear 65/70 es código nuevo y la paridad es
  cambio de comportamiento: mi «sin refactor» original era incumplible.
- Fin: grep cero de símbolos V3; compila; **gate in-game batch #1**.

### T0c — Renombrado `_TEST` → canónico · separado, y con una trampa
**`LFPG_Sorter_TEST_Kit` y `LFPG_Sorter_TEST` están en `config.cpp:1080` y `:1086`.** Son
classnames ligados a persistencia: renombrarlos rompe cualquier mundo que los tenga colocados.
Decisión explícita: conservar classname o migrar. Las clases de UI no están en config y renombran
libres — ojo a los paths de layout y a `FindAnyWidget` por nombre, que el compilador no ve.

### T2 — Autoridad de servidor · **segundo** (sube por haber jugadores)
SEC02, **+ SEC03** (añadida por el council: sin ella el reemplazo de cable queda a medias), SEC20,
SEC01. No es sorter, no se duplica, no espera a T0b. Con jugadores reales esto es superficie de
ataque, no deuda.

### T3 — Dispositivos rotos o que mienten
D04, D05, D01, D02, D03. **D16 no entra: riesgo aceptado** (ver arriba). D04 tiene doble lectura:
declarar la clase en `CfgVehicles` (arreglo funcional), o borrar el código muerto.

### T4 — Grafo, render y coste
G01 (adelantable: fichero intacto y contrastado por segunda revisión), G02, G04, G18, R02, R04,
**+ R15** (omitida), S04, S08. **S04/S08 se arreglan una sola vez en el handler compartido, viva o
no la V3.** Jubilar no los arregla: los arrastra.

### T5 — Instrumento · con dientes, no de acompañamiento
A03 + línea base del §7 + harness de inyección de fallos + re-medición de arena. **Es la puerta de
cierre de T1 y T2**, no un adorno paralelo.

### T6 — Censo de clases muertas · **descartado por decisión del dueño**
Manda mantenibilidad, no huella. Queda registrado por si el objetivo de arena vuelve: SEC18, E13,
R22, D04-código serían las candidatas, y probablemente el mejor kB/hora del plan.

### Después de T0b, nunca antes
Los P2/P3 de UI del sorter (S01–S03, S10–S12, S20–S24, dedup de `HandleSorterTest*`). Arreglarlos
ahora es escribir en código que se va a borrar.

### Las 126 fichas P2/P3
No se planifican como PRs. Al tocar un fichero de cualquier tramo, se trían las fichas de ese
fichero. 28 están vivas por construcción (con el sesgo del §1); **98 son desconocidas, no
corregidas**.

---

## 4. Lo que sigue sin verificar

- Nada bloquea el arranque: **T1 empieza ya**.
- Tres fichas necesitan ejecución in-game cuando toque su tramo, sin bloquear: G02 (cuánto persiste
  la incoherencia), SEC01 (recuento de entregas por cliente), R15 (restauración de cámara CCTV).
- **D16 queda cerrada como riesgo aceptado con control compensatorio externo**, no como corregida.
  Su reapertura depende de que siga cargado el mod de protección de bases.
- La ratio 3,0 kB/clase aplicada a estas 7 clases es proyección, no medida de este borrado.
- La dirección de la divergencia S10.
- Las 98 fichas P2/P3 en ficheros tocados.
