# LFPowerGrid — HANDOFF

<!-- LIVE-STATE:START -->
# LFPowerGrid — Estado vivo · snapshot 2026-09-08

## [2026-09-08 mañana] BACKLOG EN PARALELO: 13 LANES, LA V3 DEL SORTER JUBILADA Y LAS 101 FICHAS P2/P3 TRIADAS

**`main` paso de `8de29d5` a `adfd29c`, 19 commits.** Local, **sin push** (`origin/main` sigue en
`d61705e`). **Nada desplegado**: `P:\Mods` conserva el PBO de produccion. Codigo: 44 ficheros,
1.339 inserciones y **7.739 borrados** (casi todo la UI V3).

### LA HERRAMIENTA QUE FALTABA, Y LA ENCONTRO UNA LANE SOLA
**Existe un linter offline de Enforce y nadie lo estaba usando:**
`python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py <addon_root>`.
Cubre `.c`, `.layout`, `config.cpp`, `inputs.xml` y `.rvmat`; saca JSON con `errors` y `warnings`
en la raiz. **Es el gate estatico que este proyecto no tenia.** Dos trampas: `status` vale `WARN`
aunque `errors` sea 0 (gatea por `len(errors)`), y **su valor esta en el DELTA**, no en el
absoluto. Detalle en `~/.claude/skills/_shared/dayz-conventions.md` §Testing.

| arbol | ficheros | errores | warnings |
|---|---|---|---|
| `main` al empezar la 2ª oleada | 271 | 0 | 56 |
| `main` fusionado (`adfd29c`) | **263** | **0** | **47** |

Cero errores nuevos, y los 9 warnings menos son atribuibles: −5 `ES-GETTYPE-EXACT-MATCH` (codigo
V3 retirado) y −4 `ES-CTX-READ-UNCHECKED` (justo lo que arreglaba la rama de deuda).

### Oleada 1 — 5 lanes, 24 fichas, **las cinco VERDE**
T3 dispositivos (D01-D05), T4 grafo (G01, G02, G04, G18), T4 render y coste (R02, R04, R15, S04,
S08), T2 autoridad de servidor (SEC01, SEC02, SEC03, SEC20) y el cierre de la V4 del sorter.
1.087 lineas, cero GRAVE, 8 MEDIO y 7 MENOR anotados. Uno de los MEDIO se arreglo (`d59cad8`): al
arreglar D03, el guard de desmontaje paso a leer un getter que difiere entre cliente y servidor;
vuelve a `m_StoredEnergyX10`, la SyncVar que ven los dos.

### Oleada 2 — T0b, las dos ramas de deuda y el triaje
- **T0b: jubilada la INTERFAZ V3, NO su entidad.** ⚠ **El plan del council estaba mal en esto** y
  seguirlo habria roto la carga del mundo: `LFPG_Sorter_TEST` **hereda** de `LFPG_Sorter` en script
  (`test/LFPG_Sorter_TEST.c:15`) y en config (`config.cpp:1086`). Se borraron las 5 clases de UI y
  sus 3 layouts; **`LFPG_Sorter` y `LFPG_Sorter_Kit` se conservan**. La accion V4 pasa de
  `GetType()` exacto a `LFPG_Sorter.Cast()`, asi que **un sorter V3 ya colocado conserva su panel**
  — importante, hay jugadores reales. Solo `LFPG_Sorter_TEST` deriva de la base: la relajacion esta
  acotada.
- **`debt/v3-data-integrity`**: 20 items juzgados uno a uno — 6 PORTAR, 5 YA-RESUELTO, 2 OBSOLETO,
  **7 CONFLICTO**. Los 7 estan **todos en el camino del dinero** y chocan con la reescritura de T1
  de anoche: quedan como deuda con cita, no se portaron.
- **`maint/audit-kimi-followup`**: los 3 items portados, −75 lineas netas. VERDE sin un solo
  hallazgo.

### Las 101 fichas P2/P3, por fin conocidas
Cinco lanes de solo lectura, cero codigo tocado: **92 VIVA, 3 MUERTA, 6 DUDOSA**. El instrumento
viejo («fichero intacto ⇒ ficha viva») era optimista al reves: casi nada se habia curado de rebote.
Cada ficha viva lleva `path:line`, coste estimado y con que otras comparte fichero. Informes en
`reviews/2026-09-08-triaje-fichas-p2-p3/`. Lo que las lanes ponen primero: **S15** (mover
pertenencias por un enlace fuera de alcance), **G06** (un corte local interrumpe redes ajenas),
**D06** (sustitucion repetible sin conservar salud), **SEC10** (target ilegible deja sin cargar
cables o saldos habiendo backup).

### GATE IN-GAME: EJECUTADO Y VERDE (12:01-12:03, evidencia en `reviews/2026-09-08-arranque-verificacion/`)
**El mundo carga.** Servidor y cliente DayZDiag 1.29 sobre el PBO de `adfd29c`
(sha256 `8d558ae0…`, desplegado 11:46, arranque 12:01 — el orden importa y se comprobo).
El PBO de produccion anterior queda respaldado en
`_staging\@LFPowerGrid_PRODUCCION_98ceb1eb.pbo`.

- **Cero** `Can't compile`, `Compile error`, `CParser`, `Unknown type`, `SCRIPT (E)`, VME y
  corrupcion de modstorage, en los CUATRO ficheros de log.
- Los cuatro modulos compilan en ambos peers: **Mission 590 clases en servidor, 588 en cliente**.
- Controles positivos: `MissionServer OnInit (v1.2.4)`, `MissionGameplay OnInit (v1.2.4)`,
  `catalogValid=true currencies=4`, `DeviceInspector Widgets created`, `BTCAtmView Pre-created`.
- **Discriminador de la jubilacion**: cero apariciones de los cinco simbolos de UI V3 en runtime.
- **D04 acreditado en vivo**: el RPT trae `WORLD : Create entity type 'Land_radio_panelpas'`. La
  clase padre vanilla EXISTE, asi que el riesgo declarado («pasar de funcion muerta a error de
  config») **no se materializo**.
- **La accion V4 alcanza a una entidad V3, con control negativo**: spawneado un `LFPG_Sorter`,
  `action_use` de `LFPG_ActionOpenSorterPanel_TEST` da `condition_failed`, mientras una accion
  inventada sobre el mismo objeto da `action_not_found`. Los dos errores se distinguen, asi que la
  accion SI se resuelve contra la entidad V3; la rechaza su guard por no estar alimentada ni
  enlazada, que es la paridad D-01 correcta.

**Lo que este arranque NO acredita**, y sigue pendiente:
1. Que el panel V4 **ABRA**: hace falta un sorter alimentado y enlazado (generador + cable), que es
   una fixture, no un spawn.
2. Escalado del panel V4 a otras resoluciones y DPI: portar `LFPG_UIScaler` no acredita que escale.
3. SEC01 (recuento de entregas por cliente) y R15 (restauracion de camara CCTV).

⚠ **Trampa de infraestructura resuelta por el camino, que volvera a pasar**: el arranque murio con
`launcher_root_identity_drift` porque el registro sellado fija la identidad NTFS del directorio
`DayZ_MCP_dev\tools\native-launchers\dayz-test-v1`, y ese directorio se recreo (esta en OneDrive).
El binario era byte-identico (`sha256 ecf57545…` coincide), asi que la deriva era benigna. Remedio,
autorizado por el dueno: `replace-dayz-test-v1 --expected-sha256 <sha del registro>`. **El token CAS
va en MAYUSCULAS** (`_HEX = frozenset("0123456789ABCDEF")`): en minusculas devuelve
`invalid_launcher_registry_update` sin decir por que.

### Cola que queda
T0c (renombrado `_TEST` → canonico, con `LFPG_Sorter_TEST` ligado a persistencia en
`config.cpp:1080` y `:1086`), las 92 fichas vivas, los 7 CONFLICTO de dinero de la rama de deuda,
los 8 MEDIO de la oleada 1 y los 2 de L7, y la auditoria
`LFPowerGrid_Auditoria_2026-09-07.md`, que **sigue sin abrirse**. **P0.1 probablemente ya no es una
decision**: dos lanes independientes concluyeron que la divergencia S10 no se reproduce — las
guardas de preview son equivalentes en las dos versiones y la diferencia de energia afecta a editar
y abrir, no a previsualizar.

Evidencia completa: `reviews/2026-09-08-backlog-paralelo/`,
`reviews/2026-09-08-backlog-paralelo-oleada2/` y `reviews/2026-09-08-triaje-fichas-p2-p3/`.

---

## [2026-09-08 madrugada] REVISION CRUZADA CERRADA EN VERDE Y LAS CUATRO RAMAS FUSIONADAS A `main` LOCAL

**`main` paso de `d61705e` a `8de29d5`.** Es la primera vez en meses que se mueve. **Local: NO
se ha hecho push**, y el PBO desplegado esta **restaurado a produccion** (T1+E08, sha256
`98ceb1eb…`, verificado por hash y con los cuatro discriminadores a 0). El dueno no firmo desplegar
esto; el build del merge espera en `_staging\@LFPowerGrid_main_20260908\` (99.672.226 B,
sha256 `abb96c1c…`).

**Contrato de rondas fijado ANTES de la ronda 1** (la leccion que costo dos rondas en T2):
VERDE = cero GRAVE vivo en el camino del dinero, cada GRAVE con cita `path:line` verificable por
el receptor. Tope duro 1 auditoria + 1 correccion + 1 re-auditoria. **Se respeto: no hubo ronda 4.**

| ronda | quien | resultado |
|---|---|---|
| 1 · auditoria de T1 entero + T6 | `cursor-grok-4.6-xhigh` | VERDE, cero GRAVE, 6 MEDIO + 3 MENOR. 11 min 49 s, 101 llamadas |
| 2 · correccion | `gpt-6-astra` x `prime-agent`+`openai-codex` | 5 correcciones, 148 ins / 92 del en 8 ficheros. Commit `9d2d06a` |
| 3 · re-auditoria del parche **y** de las inserciones de T5 | `cursor-grok-4.6-xhigh` | VERDE, cero GRAVE. **C1..C5 las cinco CIERRAN.** 15 min 58 s, 248 llamadas |

**19 de 19 citas `path:line` verificadas por mi abriendo el fichero**, en las dos rondas. Ninguna
inventada. Por eso el dictamen se tomo en serio.

### Que se corrigio (todo en `9d2d06a`)
- **C1** `LFPG_FileUtil.c` — `TryReadSellDestroyIntent` normaliza con `Trim()` las cinco lineas
  antes de comparar. **Esto era lo mas serio de la ronda:** si `FGets` dejaba el `\r`,
  `lineUid != uid` fallaba SIEMPRE y la reconciliacion de E04 no destruia nunca, o sea que el
  crash entre credito y destruccion dejaba items **y** saldo. No se pudo determinar que hace
  `FGets` (no hay ningun round-trip `FPrintln`->`FGets` probado en produccion en este mod; los dos
  unicos son nuevos de T1). **Se hizo que no importe.** Firma: `enstring.c:304`.
- **C2** `lfpg_devicebase.c` — el comentario decia «Fail-closed on purpose» sobre un `return false`
  que es fail-**open**. Reescrito. Overrides nuevos de `LFPG_BlocksDismantle` en `LFPG_Battery`
  (`m_StoredEnergyX10`) y `LFPG_Furnace` (`m_FuelCurrent`).
- **C3** `LFPG_BTCHelper.c:1531-1542` — el marcador de venta se desarmaba aunque la destruccion
  quedara a medias. Ahora `return` conservandolo.
- **C5** `LFPG_BTCAtm.c` — **un ATM con saldo al que disparan perdia el dinero igual que con el
  destornillador**, y T6 solo habia cerrado el destornillador. `hitpoints=200` en los dos ATM y
  `LFPG_OnKilled` solo apagaba el LED. Ahora suelta el stock al suelo en pilas, tope 64, guard de
  una entrega por instancia y rollback si el diario rechaza el descuento. Va en `LFPG_BTCAtmBase`,
  asi que el ATM admin lo hereda. Decision de producto firmada por el dueno.
- **C4** higiene: `ref` fuera de 5 locales, `DestroyPlayerCash` borrada (cero call-sites).

### Gate de compilacion — VERDE, y con la trampa de la semana cerrada
- **Despliegue 01:39:04 -> arranques 01:45:33/35 (servidor) y 01:45:41/44 (cliente).** POSTERIOR.
- Discriminadores **en la ruta de despliegue**: `LFPG_BTC_MAX_ENTITIES_ON_KILL` 0->2,
  `kill drop stockBefore` 0->1, `LFPG_FaultInject` 0->27, `LFPG_UIPalette` 0->196,
  `LFPG_BlocksDismantle` 0->6, `LFPG_FinishWiringTxn` 0->0 (T2 sigue archivado, correcto).
- Controles positivos: `catalogValid=true currencies=4`, `MissionServer OnInit (v1.2.4)`,
  `MissionGameplay OnInit (v1.2.4)`.
- **Cero** `Can't compile`, `CParser`, `compile error` en los CUATRO ficheros de log.
- **TRAMPA QUE VOLVIO A MORDER:** un `grep` de `SCRIPT (E)` filtrado por `lfpowergrid` devuelve 1.
  **NO es del mod**: casa por la RUTA, es el ultimo frame del stack trace de `pluginitemdiagnostic`
  (ruido de vanilla) y termina en `P:\LFPowerGrid_dev\_server\mpmissions\...\init.c:918`. Abrir la
  linea, no fiarse del contador.
- Build: `probe 1: exit=0 unparseable=0`, config scan limpio con 0 apartados, AddonBuilder exit=0
  en 518 s.

### Lo que SIGUE sin probarse, y no se disimula
**Nada de esto tiene prueba de COMPORTAMIENTO in-game.** El gate prueba que compila y arranca.
En concreto: C5 (matar un ATM con saldo y ver caer las pilas) no se ha ejecutado nunca, C1 no se
ha probado con un round-trip real de fichero, y C3 no se ha ejercitado con una destruccion parcial.

### Dos MEDIO aceptados por escrito
- **Residual de C5:** spawnea antes de descontar. Un crash entre ambas cosas, si las pilas nuevas
  llegan a persistirse, deja BTC en el suelo Y stock en la maquina. **Falla hacia DUPLICADO, no
  hacia perdida**; invertir el orden falla hacia perdida, que es peor. Archivado.
- **T5 armado.** Cerrado mas firme de lo que pudo el revisor (a el le faltaba el fichero):
  `ShouldFail` y `ShouldCrash` (`LFPG_FaultInject.c`) devuelven `false` de entrada si
  `!LFPG_FAULTINJECT_COMPILE_GATE`, que es `false` fuera de `DIAG_DEVELOPER`. **En un servidor
  retail el inyector es codigo muerto en runtime aunque aparezca el JSON.**

### Aviso de merge que dio el revisor y que se comprobo despues de fusionar
El hunk de T5 esta escrito contra el arbol **pre-C3**, asi que un apply mecanico podia tirarse el
`return` de C3. **Sobrevivio:** `LFPG_BTCHelper.c:1542`, con el `ObserveReconcile` de T5 en `:1544`
— o sea detras, solo en destruccion completa.

### Suelto, a proposito
Un `ref` local en `LFPG_BalanceProvider_NativeImpl.c:2121` que el revisor encontro y **no se toco**:
el bucle estaba cerrado y no se mete codigo sin revisar despues de la ultima auditoria.

### LA COLA QUE SIGUE ABIERTA — inventariada el 2026-09-08 contra `PLAN-CONJUNTO.md`

**Aviso de lectura: hay DOS cosas llamadas «T6».** El «T6» de las ramas es el **guard de
desmontaje** (`fix/t6-dismantle-guard`, hecho). El «T6» del plan del council es **censo de clases
muertas**, y está **descartado por decisión del dueño**. No son lo mismo.

Estado real de los tramos del plan (`reviews/2026-09-06-council-auditorias/PLAN-CONJUNTO.md`):

| tramo | qué es | estado |
|---|---|---|
| **P0.1** | decisión de producto sobre la divergencia S10 (qué protección de preview gana) | **PENDIENTE — es una decisión tuya, no código** |
| P0.2 | inventario de divergencias V3↔V4 | HECHO 2026-09-07 (14 divergencias) |
| **T0a** | extracción de paleta | **HECHO** · fusionado 2026-09-08 |
| **T1** | integridad monetaria | **HECHO** · fusionado, revisado y corregido 2026-09-08 |
| **T0b** | **cerrar V4 y jubilar la V3** — 6 subtareas | **PENDIENTE. Es «el plan de recorte».** |
| **T0c** | renombrado `_TEST` → canónico | **PENDIENTE**, con trampa de persistencia |
| **T2** | autoridad de servidor | **ARCHIVADO SIN CERRAR** · 6 de 7 condiciones, V2 parcial |
| **T3** | dispositivos rotos o que mienten (D04, D05, D01, D02, D03) | **PENDIENTE**. D16 no entra: riesgo aceptado |
| **T4** | grafo, render y coste (G01, G02, G04, G18, R02, R04, R15, S04, S08) | **PENDIENTE**. S04/S08 se arreglan una vez en el handler compartido, viva o no la V3 |
| **T5** | instrumento de inyección de fallos | fusionado 2026-09-08, pero **NUNCA EJECUTADO**: el inyector no se ha armado jamás |
| ~~T6-plan~~ | censo de clases muertas | **descartado por el dueño** (manda mantenibilidad, no huella) |
| — | S01-S03, S10-S12, S20-S24 y dedup de `HandleSorterTest*` | **después de T0b, nunca antes**: hoy es escribir en código que se va a borrar |
| — | 126 fichas P2/P3 | 28 vivas por construcción, **98 desconocidas, no corregidas**. No se planifican como PR: se trían al tocar cada fichero |

**T0b, que es lo que se suele llamar «el recorte», tiene estas 6 subtareas y ninguna está hecha:**
1. aplicar las decisiones de P0 (paridad, S10);
2. emisor de `SORTER_TEST_RESYNC (65)` y productor de `SORTER_TEST_CARGO_REFRESH (70)` — el
   servidor ya los atiende, **falta el lado cliente**;
3. `LFPG_SorterView_TEST.IsOpen()` en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`;
4. retirar 9 enganches de `LFPG_MissionInit.c` y 4 handlers de `LFPG_RPCClientHandler.c`, y quitar
   el mutex anti dual-open (`LFPG_SorterView_TEST.c:1329`);
5. **decidir el destino del hook MCP** — dejarlo «porque es TEST» se come ~21 kB. *No renombrar
   `LFPG_MCP_SorterCmd.layout`*: guarda explícita de la auditoría;
6. borrar las 7 clases V3 y sus 3 layouts.

**Y ojo con lo que el merge de hoy dejó en `main`, verificado el 2026-09-08:**
- **Las DOS acciones del sorter están registradas**: `LFPG_ActionOpenSorterPanel` (V3, producción)
  en `LFPG_ActionRegistration.c:66` y `LFPG_ActionOpenSorterPanel_TEST` (V4) en `:69`. El panel V4
  es alcanzable en un build.
- **`S1_PROBE = true` está cableado a mano** (`LFPG_SorterView_TEST.c:243`) y **NO** detrás de
  `LFPG_PERFDIAG_ENABLED` (que existe y vale `false`, `LFPG_Defines.c:405`). Dispara en
  `:1562-1564` y escupe ~16 líneas al RPT **en cada apertura del panel**. Es D-05 del inventario.
- **D-02**: la V4 no lleva `LFPG_UIScaler` (sí existe, en `3_Game/`, y lo usan las clases V3). Fuera
  de 1080p al 100 % el panel V4 se ve desproporcionado. **La V4 no es apta para producción tal
  cual** — no es un problema hoy porque la V3 sigue viva y es la que sirve, pero es exactamente lo
  que T0b tiene que resolver antes de borrar la V3.
- Nada de esto está desplegado: el PBO de `P:\Mods` es el de producción (T1+E08).

**Ramas de deuda que siguen sin fusionar** (cada una 1 commit por delante, 15 por detrás de `main`):
- `debt/v3-data-integrity` (`5df9f47`) — deuda v3, **12 de 19 ítems**, tres tramos revisados.
- `maint/audit-kimi-followup` (`1d178ec`) — 3 ítems de la auditoría de sobreingeniería, revisados
  R21 por Codex, **sin compilar**. Cero valor de huella: los tres son cliente o `3_Game`.

**De T2, además del tramo entero**: los controles funcionales **V1..V7 in-game** nunca se
ejecutaron, y quedan sus dos deudas MEDIO (`R-T2-AGUA`, `R3-01`). Detalle en
`reviews/2026-09-07-t2-autoridad-servidor/README.md`.

### Notas de lane, medidas hoy
- **`prime-agent --mode rpc` LEE por stdin.** Con `-p` y stdin cerrado sale en 2 s con `rc=0` y
  cero bytes: una corrida muerta que se declara buena. La invocacion con `-p` es `--mode json` + pty.
- El aviso `Model "gpt-6-astra" not found for provider "openai-codex"` **no impide** que sirva el
  modelo: la identidad sale acreditada en el stream (`provider: openai-codex`, `model: gpt-6-astra`).
- **`prime-agent` + `openai-codex` NO re-arma la bomba de `~/.codex/config.toml`**: sha256 idéntico
  antes y despues (`f2ae2da4…`). La bomba es de `codex exec -C <dir>`, no de esta ruta.
- `--thinking` de prime-agent llega a `max`; **no existe `ultra`** en ese dial.

---

# LFPowerGrid — Estado vivo · snapshot 2026-09-07

**Helicóptero aparte — v2 (Codex, 2026-09-07):** el usuario rechazó el parecido de v1
y priorizó dos primeros planos. Refinados asiento, turbina, escape, mástil y materiales:
24.952 triángulos, 237 piezas, .blend/GLB/FBX y mapas 4K/2K en
`C:/Users/guill/Documents/Codex/LF_ScrapHeli_2026-09-07/refinement_v2/`.
Usuario confirmó solo editable/texturas, sin plataforma y <=25.000 triángulos.
UV SAT cero solapes, tangentes, reimportaciones y cribado de culling Blender verificados.
ZIP: 55 archivos, 166.823.064 bytes; v1 conservada. Parecido pendiente de revisión del usuario.
No integrado en DayZ; no toca T1 ni servidor. Handoff:
`30_Sessions/2026-09-07-scrapheli-modelo-editable.md`.

**Asset aparte para revisión (Codex, 2026-09-07):** sorter nuevo desde la imagen del usuario,
6 conexiones delante y 1 detrás, acero con desgaste leve, 6412 triángulos. Editable `.blend`,
GLB/FBX, mapas 4K/2K y renders en `C:/Users/guill/Documents/Codex/LFPG_Sorter_2026-09-07/`.
Reimportaciones y UV verificadas. Usuario pidió SOLO editable y texturas; integración DayZ
posterior. No modifica T1 ni el servidor. Handoff: `30_Sessions/2026-09-07-lfpowergrid-sorter-modelo-editable.md`.

**HAY SERVIDOR PRIVADO CON JUGADORES.** Eso reordeno el plan entero: la perdida de dinero dejo de
ser deuda latente y pasa a acumular daño por sesion. Detalle en
`30_Sessions/2026-09-07-lfpowergrid-council-auditorias-y-t1.md`.

**[2026-09-07 tarde] LOS CUATRO TRAMOS ESTAN COMMITEADOS. El gate de T2 quedo VERDE a las 20:06.**

| commit | rama | tramo |
|---|---|---|
| `34499a1` | `fix/t1-integridad-monetaria` | **T1** integridad monetaria, 7 fichas P1 |
| `8cb6222` | idem | artefactos del council |
| `8039fde` | `archive/t2-autoridad-servidor` | **T2** autoridad de servidor — **ARCHIVADO SIN CERRAR, NO DESPLEGAR** |
| `17aac47` | `feat/t0a-paleta` | **T0a** paleta extraida |
| `1f12ac0` | `feat/t5-harness` | **T5** instrumento de inyeccion de fallos |
| `a187898` | `fix/t6-dismantle-guard` | **T6** guard del desmontaje con saldo + E04 endurecido — **el guard esta VERIFICADO in-game** |

**OJO CON ESA TABLA: las ramas NO son hermanas desde `main`.** Lo corrigio la sesion del
gate y lo verifique: el padre directo de las puntas de T2, T0a, T5 y T6 es **`8cb6222`**, o sea la
punta de T1 (`git merge-base --is-ancestor 8cb6222 <rama>` da SI en todas; el merge-base con
`main` es `d61705e` igualmente en todas). Consecuencia practica: **el PBO de T2 ya contiene T1**,
asi que un arranque de T2 tambien ejercita T1 — por eso `catalogValid=true` sale en el arranque de
T2. Lo mismo vale para T6, que ademas es la unica rama con una correccion **verificada por
comportamiento** y no solo compilada.

**GATE DE COMPILACION DE T2: VERDE (2026-09-07 20:06).** Lo cerro la sesion «Cerrar el gate de
compilacion de T2/T0a/T5» sobre caja vacia, tras preguntar al dueno por los procesos ajenos en vez
de matarlos. Evidencia que dio:
- Despliegue **20:05:06** -> arranques **20:06:00 / 20:06:03 / 20:06:15**. **Posterior**, que era
  la trampa de la semana.
- Discriminador de contenido **en la ruta de despliegue** (no en staging): `LFPG_FinishWiringTxn`
  de **0 a 27**.
- Controles positivos: `catalogValid=true` (servidor) y `MissionGameplay OnInit (v1.2.4)` (cliente).
- **Cero** `Can't compile`, `CParser`, `compile error` y `not closed` en los cuatro ficheros de log.
- El `SCRIPT (E)` de `pluginitemdiagnostic` tambien sale en el arranque verde de T1 de las 12:18:
  ruido de vanilla, no de T2.
Nota operativa suya: `dayz_test_run` exige `extra_mods=['@DayZ_MCP']` o falla con
`bridge_mod_missing`. **T0a y T5 seguian en marcha** cuando se escribio esto.

**[P1 NUEVO, 2026-09-07 ~20:1x] DESMONTAR UN ATM DESTRUYE SU SALDO, sin aviso ni reembolso.**
Lo encontro la sesion del gate al ir a validar E16, y lo he verificado yo por separado leyendo el
arbol (estatico; **sin reproducir in-game todavia**):
- El ATM **es desmontable**: `lfpg_devicebase.c:592,599` da el kit por convencion
  (`GetType() + "_Kit"`), `LFPG_BTCAtm.c` **no lo sobrescribe** (cero hits de `_Kit`), y
  `LFPG_BTCAtm_Kit` existe en `config.cpp:2287`.
- El camino de desmontaje **no mira el dinero**: `LFPG_ActionDismantleDevice.c` tiene **cero**
  referencias a `BtcStock`, `Balance` o `refund`, y hace `g_Game.ObjectDelete(device)` en `:201`.
- El ATM **no se defiende**: `LFPG_BTCAtm.c` guarda `m_BtcStock` como SyncVar (`:40`) y no tiene
  `LFPG_OnDeleted` ni guarda de desmontaje. La base lo declara vacio en `lfpg_devicebase.c:582` y
  otros dispositivos si lo sobrescriben (`LFPG_Battery.c:257`, `LFPG_Camera.c:104`...).
- Accion viva: `LFPG_ActionRegistration.c:116` y `:157`.
**Con jugadores reales esto es peor que E16**: E16 exigia ausencia de 3 boots y reaparicion; esto
exige un destornillador.

**Y tumba la premisa de E16:** no existe el «ATM empaquetado en inventario» sobre el que razonaba la
ficha. `LFPG_BTCAtm` es `Inventory_Base` con `itemSize {0,0}` (`config.cpp:2305-2315`); lo que se
guarda es un kit generico distinto. O sea que la medida in-game que estaba firmada («¿conserva
`m_BtcStock` en hive al reaparecer?») **ya no es la pregunta**: la pregunta es el desmontaje.

**[GATE COMPLETO 2026-09-07 20:37] T0a y T5 TAMBIEN VERDES — las tres ramas compilan y arrancan.**
Mismo protocolo que T2, con discriminador de contenido en la ruta de despliegue y **control cruzado
en los dos sentidos**, que descarta artefacto viejo sin depender de mtime:

| rama | PBO construido | desplegado | arranques | discriminador (en despliegue) | control cruzado |
|---|---|---|---|---|---|
| T2 | ya estaba | 20:05:06 | 20:06:00/03/15 | `LFPG_FinishWiringTxn` 0->27 | — |
| T0a | 20:20:46, 99.646.619 B | 20:21:50 | 20:22:00/02/14/17 | `LFPG_UIPalette` 0->196 | `FinishWiringTxn` 27->0 |
| T5 | 20:32:44, 99.676.173 B | 20:33:26 | 20:37:10/12/24/27 | `LFPG_FaultInject` 0->27 | `UIPalette` 196->0 |

Builds con `tools/build_guarded.py --target <staging>\Addons` (sus defaults ya apuntan a
`P:\LFPowerGrid`); los dos `exit=0`, `unparseable=0`, 0 configs apartadas. Cero `Can't compile`,
`CParser` y `compile error` en los cuatro ficheros de cada arranque, y **ninguna linea `(E)` nombra
un script del mod**. Los ficheros de cada run los dio el propio puente (`wait_for.scanned`), no yo:
hubo dos arranques de servidor cerca en T5 y no habia que adivinar cual era.

**TRAMPA DE LECTURA, para que nadie lo lea como rojo dentro de un mes:** en el servidor de T0a hay
**1** coincidencia de `not closed`. **NO es** el `CParser: quoted string not closed` de Enforce
(`CParser` = 0). Es un WARNING de CommunityFramework —
`File "$mission:storage_1/communityframework/modstorageplayers.bin" was not closed` — por parar el
servidor sin gracia. Sale igual en el arranque verde de T1 de las 12:18.

**Positivo de runtime inesperado de T5:** el servidor imprime
`[ERR] LFPG_FAULTINJECT DISARMED reason=no_file`. El inyector no solo compila: **se ejecuta** y se
declara desarmado. Aun asi — y esto hay que leerlo entero — **el arranque prueba que T5 COMPILA, no
que el inyector FUNCIONE**; para lo segundo hacen falta el JSON, la `armPhrase` y un escenario, y
eso NO se ha ejecutado. (El `#ifdef DIAG_DEVELOPER` de `LFPG_FaultInject.c:17-21` solo elige el
valor de una constante; los cinco usos son `if` de runtime en `:55,:62,:78,:95,:119`. No hay codigo
excluible, asi que el verde no es vacio: comprobado con `git diff T1..T5 -- 'scripts/*.c'` filtrando
directivas, que devuelve esas 3 lineas y ninguna mas en todo el tramo.)

**[E16 / P1 DEL DESMONTAJE — QUE SE MIDIO Y QUE NO, 2026-09-07 20:4x]** Lee los dos apartados: el
hallazgo subio de categoria, pero **no** esta reproducido de punta a punta.
- **MEDIDO — el mod OFRECE el desmontaje sobre un ATM CON saldo dentro.** Se monto el escenario
  entero in-game: `LFPG_BTCAtmAdmin` spawneado, 3 `AmmoBox_9x19_25rnd` (el `btcItem` de esta caja)
  al inventario, y deposito REAL por la UI (`ui_set_text` en `EditBtcAmount` + `ui_click` en
  `BtnDepositBtc`, raiz `BTCAtmRoot`) -> servidor `[BTCDeposit] player deposited 3 BTC into pool`,
  cliente `[BTCTxResult] type=4 err=0 stock=3`. Con el cajero ya a **stock=3**, `action_use` de
  `LFPG_ActionDismantleDevice` devuelve `started=1`: la condicion **acepta un ATM con dinero**.
- **CONTROLES NEGATIVOS, para que los verdes valgan.** Sobre `LFPG_BatteryAdapter`, que devuelve
  kit vacio (`LFPG_BatteryAdapter.c:85-89`), la misma llamada da `condition_failed`. Y `world_spawn`
  de una clase inventada da `unknown_type`, mientras `LFPG_BTCAtm_Kit` spawnea (`ok:1`). Los
  instrumentos PUEDEN salir rojos.
- **NO MEDIDO — la destruccion del saldo.** La accion lleva barra de 5 s: `action_use` la arranca
  pero no la completa, y `key_press` no sostiene teclas ("not OS input, key-up, hold"). El objetivo
  seguia en el mundo tras dos intentos (`entities_query`, 0.003 m). O sea: **desmontable con saldo =
  MEDIDO; perdida del saldo = INFERIDA del codigo**. Ficha del buzon: `fb-20260907-184749-3fc1`.
- **NO MEDIDO — la persistencia del stock entre reinicios**, que era la mitad util de E16. Dos
  causas nombradas y **ninguna atribuible al mod**: `world_spawn` usa banderas por defecto y el
  objeto no es persistente, y `dayz_test_stop` no cierra con gracia (lo dice el propio log del
  arranque siguiente), asi que el mundo nunca se guarda. **No concluir de aqui que
  `LFPG_OnStoreSaveExtra` falle: no se probo.** Para probarlo hace falta desplegar el ATM desde su
  kit (otra accion con barra, mismo bloqueo) y una parada graciosa.
- Firmas releidas y verificadas al hacerlo: `LFPG_BTCAtm.c:273` (`LFPG_OnStoreSaveExtra` escribe
  `m_BtcStock` el primero, `:275`) y `:280` (`LFPG_OnStoreLoadExtra`, que loguea
  `[LFPG_BTCAtm] Loaded: stock=…` en `:305-311` — **ese log es el instrumento** para medirlo el dia
  que se pueda).

**Estado de la caja al cerrar la sesion del gate:** PBO de produccion **restaurado** (T1+E08,
sha256 `98ceb1eb…`, 99.641.798 B, verificado por hash, con los tres discriminadores de tramo a 0 y
`catalogValid` intacto) y sin procesos DayZ vivos. La rama se devolvio a
`archive/t2-autoridad-servidor` en ese momento — **pero OJO, ya no es asi**: despues se creo
`fix/t6-dismantle-guard` para las dos correcciones de abajo, y el arbol se quedo ahi con cambios
sin comitear. La deuda 3 del README de T2 («nada de este tramo se ha compilado ni arrancado
nunca») queda **cerrada en su mitad de compilacion**; los controles funcionales V1..V7 in-game
siguen pendientes.

**[2026-09-07 22:5x] Dos correcciones escritas, COMPILADAS y — el guard — VERIFICADAS IN-GAME.**
Ambas decididas por el dueno tras plantearle el residuo real. **COMMITEADAS en `a187898`**, rama
`fix/t6-dismantle-guard` desde `8cb6222` (la punta de T1), 4 ficheros y 91 inserciones. Sin fusionar
y sin publicar, como el resto: `main` sigue en `d61705e`.
1. **Guard fail-closed del desmontaje** (cierra el P1 de arriba). Virtual `LFPG_BlocksDismantle()`
   en `lfpg_devicebase.c` — por defecto `false`, asi que ningun otro dispositivo cambia —,
   `override` en `LFPG_BTCAtmBase` devolviendo `m_BtcStock > 0`, y la comprobacion en
   `LFPG_ValidateDismantle` junto a las de adjuntos y cargo. El jugador tiene que vaciar el cajero.
2. **E04 endurecido.** Mapa `s_SellDestroyedThisBoot` (mismo patron que los tres `s_…ThisBoot` de
   `LFPG_BalanceProvider_NativeImpl.c:31-33`), marcado **antes** de destruir en los dos caminos
   donde la venta completa, y guarda en `ReconcilePendingAccountSell` que sale fail-closed sin
   destruir si el marcador sobrevive a una venta ya destruida en este arranque. **Ojo al elegir
   esto**: «marcar el marcador como ya destruido» NO era nuevo — es el re-armado de R3, que ya
   estaba; y cualquier variante de esa idea es otra escritura al mismo fichero que acaba de fallar,
   asi que no cierra nada. Por eso la guarda es EN MEMORIA. No persiste a proposito, y a traves de
   un reinicio sigue valiendo solo el log fail-closed que nombra el `.sell` al admin.
**Estado**: `build_guarded.py` exit 0, `unparseable=0`, PBO **99.651.067 B** a las 21:32:30 en
`_staging\@LFPowerGrid_t6full_20260907\`, con `LFPG_BlocksDismantle`=4 y
`s_SellDestroyedThisBoot`=6 verificados por contenido.
**GATE DE COMPILACION VERDE**: desplegado **22:47:33** -> arranques **22:47:42/44** (server) y
**22:47:56/59** (client), posteriores; cero `Can't compile`, `CParser` y `compile error` en los
cuatro ficheros; `catalogValid=true`; ninguna linea `(E)` nombra un script del mod.
**EL GUARD, MEDIDO IN-GAME sobre el MISMO objeto** (`LFPG_BTCAtmAdmin` id 206, mismo build, mismo
destornillador en manos, misma distancia 1.95 m; solo cambia el stock):

| # | stock | `action_use` de `LFPG_ActionDismantleDevice` |
|---|---|---|
| 0 | 3 (build **sin** el arreglo, 20:4x) | `started=1` ← el agujero |
| 1 | 0 | `started=1` — no rompe el desmontaje normal |
| 2 | 3 (deposito real por UI) | **`condition_failed`** — cerrado |
| 3 | 0 (tras retirar los 3 por UI) | `started=1` — **vuelve solo** |

La fila 3 es la que cierra la duda: entre la 1 y la 2 se abrio y cerro la UI, asi que hacia falta
demostrar que el `condition_failed` era el stock y no algo roto por el camino. Es reversible, luego
la variable es el stock.
**E04 NO tiene prueba de comportamiento**: solo compila. Su guarda solo se observa provocando un
doble fallo de escritura sobre el `.sell`, que no se puede inducir desde el puente.
**Medido de paso, y util**: la caja es exclusiva **NO por puerto** — `port=2322` con el 2302 ajeno
se rechaza igual (`active_run_exists`), pese a que el `hint` de la herramienta sugiere "pass another
port=". Y mientras otra sesion tenga un run con `@LFPowerGrid`, el PBO no se puede ni copiar
(`Device or resource busy`): `wait_for_box_s` no cubre ese caso.
**Estado final de la caja**: PBO **devuelto a produccion** (T1+E08, hash `98ceb1eb…`, los dos
simbolos nuevos a 0) porque el dueno no ha firmado desplegar esto; el PBO verificado espera en
`_staging\@LFPowerGrid_t6full_20260907\`. Cero procesos DayZ vivos.

**T2 esta archivado, no terminado.** 3 rondas de implementacion (Grok x Cursor) y 3 dictamenes
adversariales (gpt-6-astra x codex exec). El VERDE, fijado por escrito antes de la ronda 3, quedo
en **6 de 7**: solo V2 parcial. Sin GRAVE nuevo, sin crash, sin corrupcion. Deuda viva y lo que
NO hay que reabrir: `reviews/2026-09-07-t2-autoridad-servidor/README.md` del repo de codigo.
**Las 4 fichas de seguridad (SEC01, SEC02, SEC03, SEC20) si estan atendidas.**

**Decision de producto firmada por el dueño:** al chocar con el cable de otro jugador, el
reemplazo **aborta la operacion entera**. Solo cambia algo en servidores con
`AllowCutOthersWires=false` — que eran justo los que creian tenerlo protegido y no lo estaba.

**Leccion de proceso del dia, que costo dos rondas:** partir T2 **por ficheros** para evitar
conflictos de fusion escondio los riesgos, porque vivian en la costura entre lanes; y un veto de
reparto no cambia el comportamiento exigido al sistema. Ademas las rondas 1 y 2 corrieron **sin
VERDE ni tope de rondas** definidos antes de empezar, contra la regla de la casa: el revisor puso
el liston y siempre podia encontrar un escenario mas. La ronda 3 llevo el VERDE por delante y
produjo una decision acotada a la primera.

Detalle: `30_Sessions/2026-09-07-lfpowergrid-t2-t0a-t5.md`.

**La friccion del MCP de hoy YA TIENE FICHA: `fb-20260907-163855-c261`.** Archivada con el daemon
todavia caido, lo que **desmiente lo que escribi antes aqui**: `pipeline_feedback` no pasa por el
daemon, escribe directo a `%LOCALAPPDATA%\DayZ_MCP\inbox\feedback.jsonl` y sobrevive a la caida.
El buzon NO queda incomunicado.

**[2026-09-07 ~18:5x] CAUSA RAIZ ENCONTRADA Y ARREGLADA — y NO era mi hipotesis.** La midio la
sesion «Retomar los tickets del buzon MCP»: `~/.codex/config.toml:1543` tenia
`tool_timeout_sec = 604800.0` y el gate de procedencia exigia un entero. Mismo presupuesto, distinta
grafia -> `daemon_provenance_conflict` -> el sidecar `--client` moria al arrancar. **Lo dispara
cualquier `codex exec -C <directorio nuevo>`**: el CLI de Codex reescribe `config.toml` para anotar
su `[projects.'<ruta>']` y el round-trip TOML convierte `604800` en `604800.0`. O sea que **delegar
en Codex re-arma la bomba**, y ya habia explotado igual el 2026-08-24 (`fb-20260824-000930-7af8`),
parcheada a mano quitando el `.0` — parche que caduca en la reescritura siguiente. Arreglo durable
en el commit `a002777` de `work/inbox-20260830-modules`: el predicado compara por VALOR, no por
grafia. **Verificado por mi solo dos cosas**: el literal (`:1543` con `604800.0`, y un segundo en
`:1568` con `180`) y el EFECTO — `bridge_status` paso de `daemon_unavailable` a devolver payload
completo, `daemon_generation` `7f8fbacd95e2...`. El predicado y sus tests no los he leido.

**El mapa completo son TRES tramos, y el tercero no lo arregla nadie todavia:** (1) el
`--idle-timeout 600` explica que el daemon DESAPAREZCA; (2) el `daemon_provenance_conflict` de
arriba explica que NO PUDIERA VOLVER; y (3) **tras un renacimiento del daemon no queda ligado NADA
de la generacion anterior** — ni el cliente MCP ni los peers del juego —, **y son dos remedios
distintos**. Medido a las ~19:0x con el daemon ya sano: `session_status` me seguia dando
`client_policy_untrusted_open_new_session` (remedio: sesion nueva; una compactacion no vale), y a la
vez `server_peer`/`client_peer` polleaban a 0,10 y 0,27 s con `binding_state: unbound_after_restart`,
`instance_prefix: null`, `ready:false`, y `lifecycle.runs: []` con `runs_retired: 185` (remedio: un
run gestionado que ligue, NO otra sesion). `unaccredited_polls_by_class.unbound_after_restart` paso
de 134 a 1912 en 25 min: los peers pollean y se les rechaza en bucle. Dos mediciones independientes,
la mia y la de la sesion del buzon MCP, con la misma `daemon_generation`.

**[CORRECCION ~19:07] De QUIEN es la caja: inferi mal, y me lo tumbaron con una medida de un
minuto.** Escribi que esos DayZ eran huerfanos de un run anterior al reinicio del daemon, deducido
de `runs: []` + `runs_retired: 185`. **Es falso**, y habia dos comandos que lo decian:
- daemon **18:56:55** · `DayZDiag 57592` **18:59:23** (servidor, puerto 2302, 5,28 GB) · `32468`
  **18:59:47** (cliente, 3,85 GB). Arrancaron DOS MINUTOS Y MEDIO DESPUES del daemon, asi que no
  pueden ser restos de antes del reinicio.
- La linea de comandos lo zanja: `-config=P:\DayZ_MCP_dev\_server\serverDZ.cfg`,
  `-profiles=P:\DayZ_MCP_dev\_server\profiles`, `-mod=...;P:\Mods\@SUB_BRZ`. **Es la sesion
  «Subaru BRZ UI interfaces», que esta viva.** Y en esa lista **no hay `@LFPowerGrid`**: no es
  nuestra, ni de la sesion del gate.

**La caja no esta sembrada de restos: esta EN USO por otra sesion trabajando.** Ademas
`lifecycle.runs: []` es lo que ve un cliente SIN acreditar — no prueba que no exista run gestionado.
Consecuencia para quien retome el gate: **no hay nada que limpiar**. Se hace cola con
`wait_for_box_s` y se lee el `session_status` PROPIO, que es el unico que ve `box.ports_in_use` de
verdad. `dayz_test_run` rechaza lanzar sobre un puerto que retiene un proceso ajeno
(`active_run_exists`, `reason port_in_use_foreign`). Matar procesos DayZ a mano lo prohibe el
runbook, y aqui ademas habria destruido el trabajo de otra sesion. El lease SI estaba libre
(`coordination.claimable: true`, cola vacia): el problema es la caja, no el lease. Y si sale
`ready:false` con `unbound_after_restart`, **no es que el arreglo del daemon haya fallado**: es el
tramo (3), y se confunde facil con un fracaso del arreglo.

**Dato de contrato, corregido por la sesion del buzon:** la adopcion admite DOS estados, no uno —
`_ADOPTABLE_STATES = frozenset({"RUNNING_IDLE", "UNRECONCILED"})` en `process_lifecycle.py:331`.

**[2026-09-07, al retomar] CUIDADO CON `REVIEW-T1.md`: esta DESFASADA para E04 y E16.** Su
timestamp es 01:22; los briefs R2 y R3 son de 01:27 y 01:50 y el informe final de 01:56. Las dos
fichas que ese dictamen deja como GRAVE **ya estan cerradas en el arbol**, verificado leyendo el
codigo el 2026-09-07:
- **E04** — diario de intencion durable: `WriteSellDestroyIntent` (`LFPG_BTCHelper.c:1859`) se
  escribe ANTES del `AddBalance` de `:1871`, y `LFPG_MissionInit.c:47` (`InvokeOnConnect`)
  reconcilia en cada conexion: saldo `before+credit` -> destruye; saldo `before` -> limpia; otro
  valor -> no toca items. El re-arme que pidio R3 esta en `:1397-1411`: si el borrado falla,
  reescribe el marcador con el saldo actual, asi que la reconexion siguiente entra por la rama que
  limpia y **no vuelve a destruir**. Doble fallo (borrar Y reescribir) sale fail-closed con
  instruccion al admin.
- **E16** — la poda del tombstone **ya no existe**: `LFPG_BalanceProvider_NativeImpl.c:1389-1396`
  devuelve `false` sin podar al llegar a 3 boots, con el comentario que lo explica (*el recuento de
  boots no es prueba de hive*). El `PersistRemoveClaimAt` que citaba la revision vive en otra
  funcion (`ObserveAbsentPendingClaimAt`, `:1442`) y solo corre con `debit == 0`.

**Lo que SI sigue abierto es peor que las dos fichas: ese codigo no lo ha revisado nadie.** No hay
ningun dictamen posterior a R3; el unico es el de las 01:22, que es el que ya no aplica. Y el
propio informe lo firma en su `LO_NO_VERIFICADO`: ninguna prueba in-game se ejecuto y no se
inyecto crash entre `AddBalance` y `DestroyPlayerItems`. O sea: **cerradas segun el implementador,
en codigo sin revisar y sin compilar**. Al retomar, el orden es gate de compilacion -> revision
cruzada de lo que R2/R3 escribieron -> recien entonces firmar residuos.

**Decisiones del dueno, 2026-09-07:** E04 firmada como riesgo aceptado y **E16 medir primero** —
pero las dos sobre la descripcion DESFASADA. Hay que re-preguntarle con el residuo real. Lo unico
que sigue mereciendo medida in-game es si un ATM empaquetado conserva `m_BtcStock` en hive: ya no
decide si hay bug, pero valida el supuesto sobre el que descansa toda la reconciliacion.

- **T1 (integridad monetaria) COMMITEADO el 2026-09-07 en `34499a1`.** Rama
  `fix/t1-integridad-monetaria` desde `421cabb`: 997 inserciones / 159 borrados en 5 ficheros
  (`LFPG_BTCHelper.c`, `LFPG_BalanceProvider_NativeImpl.c`, `LFPG_FileUtil.c`, `LFPG_BTCConfig.c`,
  `LFPG_MissionInit.c`). Tres pasadas de Grok con revision adversarial de Kimi entre medias.
  **Gate de compilacion VERDE**, con el discriminador `catalogValid=true` en la linea de config
  (campo que solo existe en T1) — **pero OJO con QUE arranque lo probo, porque lo atribui mal**:
  **[RETRACTADO 2026-09-07] el arranque de las 02:17 NO mostro `catalogValid`.** Su linea acaba en
  `currencies=4` (`_server/profiles/script_2026-09-07_02-17-05.log`) porque corrio **antes del
  despliegue**: el backup se llama `pbo-predeploy-20260907-0220`, o sea que el PBO con T1 aterrizo
  a las **02:20, tres minutos despues**. `v1.2.4` no discrimina — el build del 06-sep 20:59 ya era
  1.2.4 sin T1. Quien SI cierra el gate es el arranque de las **12:18 de hoy**
  (`script_2026-09-07_12-18-48.log`), mismo `-mod=P:\Mods\@LFPowerGrid` y mismo PBO
  (99.645.822 B, mtime 02:13:46, sin tocar desde entonces): ahi la linea acaba en
  **`... currencies=4 catalogValid=true`**. Ese arranque **no lo lance yo**; el log esta en disco.
  Tercera capa de la misma trampa: primero fue el PBO viejo, luego el artefacto equivocado, y ahora
  **el arranque anterior al despliegue**. La pregunta de cierre no es «¿verifique el fichero que el
  proceso abre?» sino **«¿corrio el proceso DESPUES de que el fichero llegara?»**.
- **[RESUELTO 2026-09-07] E08 ya no depende de inspeccionar JSONs ajenos.** El dueño corrigio la
  premisa: **el mod corre en MUCHOS servidores** y sus `LF_BTCAtm.json` son inalcanzables, asi que
  el gate no puede ser «revisar el fichero de produccion» sino **que el codigo aguante ficheros que
  nunca veremos**. Tambien corrigio mi alarmismo: que un JSON roto tumbe un mod es el estandar del
  ecosistema y nadie se sorprende. Lo que esa norma NO cubre, y era el defecto de verdad: **el mod
  escribia su propio fichero de defaults y luego lo rechazaba**. `Load()` hacia `Save()` con
  `Paper_Bill_100/50/10/1` y tres lineas mas abajo `ValidateAndClamp()` los invalidaba por R8:
  instalacion nueva, admin sin tocar nada, efectivo apagado.
  **Medido**: `Paper_Bill_100` da **0 hits en los 124 PBO** de un DayZServer (control positivo
  `Battery9V`=4) y el `config.cpp` del mod tampoco lo declara.
  **Arreglado**: el catalogo **sale VACIO a proposito** (`LFPG_BTCConfig.c:72-85` explica por que,
  para que nadie vuelva a poner cuatro clases inventadas) y hay un bloque verbose
  `LogCatalogHelp()` que se imprime ante CUALQUIERA de las 9 reglas: que queda desactivado, la ruta
  del fichero, que no hay hot-reload, la forma esperada y las 6 condiciones — con el
  `btcItemClassname` real interpolado. **Verificado en caja con los dos controles** (PBO 13:45,
  arranques 13:51 y 13:52, POSTERIORES): catalogo vacio -> bloque completo y
  `currencies=0 catalogValid=false`; catalogo valido -> **cero** bloque y `catalogValid=true`.
  Cero `Can't compile` y cero `CParser`; los unicos `SCRIPT (E)` son el stack trace de VPP
  AdminTools que sale en todos los arranques. Staging:
  `_staging\@LFPowerGrid_t1e08_20260907\` (99.641.798 B), desplegado y verificado por contenido.
  **El unico que pierde** es una instalacion NUEVA que ademas cargue un mod de economia que si
  aporte `Paper_Bill_*`: a esa los defaults viejos le habrian valido y ahora tiene que configurar.
  Los servidores ya existentes **no se enteran** — tienen su fichero en disco desde su primer
  arranque, asi que los defaults no se usan.
- **El `LF_BTCAtm.json` de produccion — contexto historico, ya no es el gate.** La validacion
  nueva invalida el catalogo ENTERO ante **una sola** entrada mala (duplicado, classname
  inexistente, solape con `btcItemClassname`, value fuera de [1,1e7], >16 entradas) y todas las
  ops de efectivo fallan cerradas. **Sin hot-reload**: `Load()` corre una vez desde
  `LFPG_NetworkManagerImpl.c:534`, corregir el JSON exige reiniciar. Los 4 defaults `Paper_Bill_*`
  **no existen en vanilla**. La caja de pruebas usa `Battery9V/Screwdriver/DuctTape/Nail` y valida;
  eso NO dice nada del servidor con jugadores.
  **[2026-09-07] Ya hay validador offline: `tools/validate_btcatm_json.py`** — reproduce las 9
  reglas citando `LFPG_BTCConfig.c:linea`, y esta verificado con control positivo (rc=0 sobre el
  JSON que el juego declaro valido), negativo (rc=1 con duplicado por mayusculas, solape,
  classname vacio y `value=0`) e indeciso (rc=2). **R8, la existencia de la clase, NO se decide
  offline** — `ConfigClassExists` mira el `Cfg*` del conjunto de mods CARGADO —, asi que sale
  DESCONOCIDA y nunca aprobada; con `--classes lista.txt` si la decide. **El JSON de produccion no
  esta en este disco**: no hay ninguno bajo el `DayZServer` retail, hay que traerlo.
- **La auditoria grande NO estaba desfasada.** `main` nunca se movio de `d61705e`, que es el
  merge-base exacto de las cuatro ramas. De sus 25 fichas P1: **24 vivas + 1 parcial, cero
  corregidas**.
- **La huella NO es el premio, y esta medido.** Hueco de arena 885 kB; jubilar la V3 rinde ~18 kB
  netos (~2 %), y la V4 es MAS GRANDE que la V3 (10 clases contra 7). Decision del dueño: manda
  mantenibilidad; el hook MCP se queda pero GATEADO.
- **Jubilar la V3 sin propagar dos cosas es una regresion silenciosa** (14 divergencias en
  `reviews/2026-09-06-council-auditorias/INVENTARIO-DIVERGENCIAS-V3-V4.md`): la accion V3 exige
  `powered`+`linked` y la V4 no (D-01), y **`LFPG_UIScaler` existe en V3 y NO en V4** (D-02) —
  fuera de 1080p el panel V4 se ve desproporcionado. Ademas `S1_PROBE` **no esta detras de
  `LFPG_PERFDIAG_ENABLED`** y escupe ~16 lineas al RPT en cada apertura.
- **D16 es RIESGO ACEPTADO, no corregido**: el codigo no comprueba propiedad ni candado, pero el
  servidor carga @BaseBuildingPlus y @Code Lock y el radio es de 2 m
  (`LFPG_DC_PAIR_DIST_SQ_FENCE = 4.0`, al cuadrado). **Si se retira ese mod, vuelve a ser explotable.**
- **Verifica el fichero que el proceso ABRE, no el que produjiste.** Verifique por contenido el PBO
  de staging y arranque el servidor creyendo que probaba el codigo nuevo; el juego cargaba el del
  dia anterior desde `P:\Mods\@LFPowerGrid\Addons\`. Se detecta en la linea `-mod=` del RPT.
- `P:` esta dentro de OneDrive pero **`P:\Mods` es un junction** a
  `C:\Program Files (x86)\Steam\...\!Workshop`. Comprobar `subst` + LinkType antes de escribir ahi.

- **Causa raiz del PBO fantasma.** `tools/build_guarded.py` limpiaba el arbol `-temp` con
  `shutil.rmtree(temp, ignore_errors=True)`; el borrado fallaba en silencio y AddonBuilder
  regeneraba solo `config.bin` y `texHeaders.bin`, empaquetaba el `gui/`, `scripts/` y `data/`
  del **2026-08-29** y decia `Build Successful`. **Nada de `sorter/v4-finish` posterior al
  29-ago habia estado nunca dentro de un PBO desplegado**: ni S1 (`7e86ca8`), ni el layout
  820x600 (`fb6bcef`), ni el hook de comando (`c4ac943`), ni el bump a 1.2.4 (`2e8ef7d`).
  El desplegado era **1.2.3**. Arreglado con un guard fail-closed (borrar, verificar, abortar
  con RC=4), verificado por control positivo. **Sin commitear** en `LFPowerGrid_dev`.
- **PBO nuevo desplegado y verificado POR CONTENIDO**, no por fecha: `TEST_SorterRoot` x1,
  `LFPG_MCP_SorterCmd` x6, `LFPG_VERSION_STR = "1.2.4"` x1 y `= "1.2.3"` x0. 99.607.143 B —
  **8,4 MB MENOS** que el fantasma, porque el temp sin limpiar acumulaba restos. Backup del
  anterior en `_backups/pbo-predeploy-20260906-2050/`. **Va SIN FIRMAR**: la privada `Return0`
  no esta en esta maquina (solo aparece `SUB_BRZ_v1`). El `.bisign` viejo quedo apartado como
  `.stale-20260906`.
- **El renombrado esta commiteado (`421cabb`) Y VIVO**, y el smoke dio seis verdes: el panel
  abre (`[SorterView] Opened for:`), `root:"TEST_SorterRoot"` resuelve sin ambiguedad
  (`/@0/TEST_SorterRoot@0`), un nombre que COLISIONA acota bien (`BtnCloseX` -> `.../HeaderFrame@0/
  BtnCloseX@0`), el clic llega a V4 (`clicked=1, handler=LFPG_SorterView_TEST, user_id=507`),
  el guard corta sin corriente (cero `REQUEST_SORT` en 556 lineas, con control positivo valido)
  y **el panel DIBUJA** — captura en `round-s3b/capture_20260906_210940_384.jpg`, 820x600 @
  550/240. El `not_handled` historico esta muerto. Cliente cerrado limpio, cero minidumps,
  **cero errores de LFPowerGrid** en 20 min con el panel abierto.
- **`dayz_test_run(build=true)` sigue ROTO** (`NativeLauncherBackendError`, ficha
  `fb-20260904-224756-ae65`) pero **YA NO BLOQUEA**: se empaqueta a mano con
  `python tools/build_guarded.py --source P:\LFPowerGrid --target <staging>\Addons`.
- **El livelock P6 esta ARREGLADO**: `session_acquire_wait` devuelve `adopted_run {ok, state
  RUNNING, dispatchable}`. El workaround de `/lifecycle/adopt` ya NO hace falta.
- **Secuencia obligatoria del puente**: `dayz_test_run` -> `session_acquire_wait` -> `wait_for`.
  `dayz_test_run(mode=client)` SUELTA el lease, el run queda `RUNNING_IDLE` y la valla rechaza
  hasta las LECTURAS con `run_not_owned`. Sondear antes de adoptar da `remote_error` pelado
  (ficha `fb-20260906-193626-f45d`, RESUELTA por la sesion del buzon en `8f5727f`; el mensaje
  bueno solo se ve en sesiones NUEVAS). Y `mode=all` esta muerto desde `1cb90b5`
  (`replace_witness_missing`): usar `mode=server` + `mode=client(run_id)`.

**Agenda de pruebas de UI: `round-s3b/UI-TEST-BACKLOG.md`** (2026-09-06). El sorter TEST esta
casi exprimido; lo que queda de el es deuda de cierre, no aprendizaje. El backlog ordena lo que
SI ensena, por lo que el instrumento alcanza: **Tier A** se prueba YA sin build, porque
`ui_reload_layout` con `$profile:` relee de disco (A1 calibrar el preview offline contra el
motor; A2 la resolucion de verdad relanzando con otro -x/-y; A3 los fallbacks de la textura
procedural, que arregla un bug vivo; A4-A7 unidades anidadas, que widget dibuja,
visible vs visible_hierarchy, y hit-testing con solape). **Tier B** espera al build roto
(`fb-20260904-224756-ae65`) e incluye las ~30 afirmaciones sin citar de `admin-ui-patterns.md` y
`lbgroups-patterns.md`. **Tier C** es tu ojo. Cada entrada trae su discriminador: sin el, no
entra.

**Ultima verificacion real:** 2026-09-04, **la ventana in-game S3-B SE EJECUTO** (run
`324f44b7`, 13:56-14:25, DayZ 1.29.163709, PBO `08FFCD14E320F5EA` sin rebuild). Resultados
medidos en `reviews/2026-08-30-uiclick-collision/round-s3b/RESULTS.md`. Cerrada con
`session_release` + `dayz_test_stop`; caja liberada.
**Nada del track de deuda v3 esta compilado**: AddonBuilder no compila los `.c`.

bugs: sin contador vivo · `LFPowerGrid_dev/bugs.md` no existe a 2026-09-01 · el detalle
de defectos vive en `10_Projects/LF_PowerGrid/bug-ledger.md`
ciclos_en_este_objetivo: 0 — **GATE DZ-R5 CERRADO el 2026-09-04**: la ventana se ejecuto.
  El bloqueo que lo abrio, `launcher_root_identity_drift` (`fb-20260831-133546-c893`), esta
  RESUELTO y ya no figura en el buzon. Lo que bloqueaba el 2026-09-04 era otra cosa y tenia
  25 min de vida: `fb-20260904-112553-f70b`, livelock de "P6 estricto" (todo run nace
  RUNNING_IDLE, sin dueno, y ningun verbo de puente despacha). Salida SIN tocar codigo,
  medida y archivada en `fb-20260904-120219-92e5`; receta en round-s3b/00-adopt-workaround.json.
  Si vuelve a aparecer: adoptar el run por /lifecycle/adopt y re-adoptar tras cada caducidad
  de lease (120 s) o session_release.

## Estado actual

**Frente A — Sorter V4 TEST (rama `sorter/v4-finish`, `c4ac943`).** Mecanica y layout
integrados; falta la sentada. `reviews/2026-08-30-uiclick-collision/S3B-PLAN.md` tiene el
plan con predicciones pre-registradas y al lado un ledger de 12 puertas (`GATES.md`, 8
ejecutables + 2 manuales + 2 abandonadas con razon) con oraculos Node en `gate-scripts/`.
**Re-verificado 2026-09-02** desde su carpeta real: `--status` da 12 puertas / 0 met /
2 abandonadas / 2 manuales sin atestar, y los 8 oraculos dan selftest 0, PASS 0, FAIL 1 y
ausencia-de-evidencia 1. El instrumento sobrevivio el viaje por OneDrive.
**Ejecutado 2026-09-04** (`round-s3b/RESULTS.md`): estados VERDE los cuatro exactos ·
geometria 1080 VERDE exacta (820x600 @ 550/240, cross-validada por dos instrumentos) ·
fidelidad 1080 8/8 rasgos · lifecycle VERDE en su mitad falsable · RPT ROJA por la letra
del gate con causa **vanilla** (`DayZPlayerInventory.HandleHandEvent`, cero frames LFPG) ·
geometria 720 INCONCLUSA y prod-vs-TEST disuelta, las dos por hallazgos de metodo (abajo).
**Ninguna puerta del ledger se ha cerrado con --attest ni --reverify**: los oraculos esperan
nombres de fichero que esta ventana no produjo tal cual, y forzar la correspondencia seria
fabricar evidencia. Hay MEDIDAS; casarlas con el ledger es trabajo aparte.

**Frente B — deuda v3 (rama `debt/v3-data-integrity`, `5df9f47`).** 12 de 19 items
cerrados (11 con codigo + err=11 refutado), 902 inserciones / 113 borrados sobre la RC.
Los 7 restantes estan rio abajo del journal, y el journal depende de una sonda de motor.
Lleva ademas el fix de version `LFPG_VERSION_STR = "1.2.4"`, que vivia SOLO en la rama del
sorter: un fast-forward de `main` desde la RC lo habria perdido.

**Frente C — huella.** El hueco es **885 kB**, no 355. Ver Invariantes.

**Frente D — mantenimiento (rama `maint/audit-kimi-followup`, `1d178ec`).** Los 3 items
de la auditoria de sobreingenieria que se podian cerrar sin la ventana: guard de debug en
`LFPG_Telemetry`, `ResolveDeviceEntity` muerto retirado, y migradores vacios inlinados.
57 inserciones / 123 borrados sobre la RC. Revisado R21 por Codex (SOUND-with-fixes,
0 FAIL). **Cero valor de huella**: los tres son cliente o `3_Game`. Sin compilar.

## Issues abiertos

No listar bugs aqui. Detalle en `10_Projects/LF_PowerGrid/bug-ledger.md`; el backlog de la
auditoria de sobreingenieria esta en su seccion `2026-09-01`.

### BACKLOG — auditoria nueva SIN LEER (anotada 2026-09-08)

`LFPowerGrid_dev/LFPowerGrid_Auditoria_2026-09-07.md` — 22.141 B, 136 lineas,
sha256 `84d1392bf4150a3d…`, mtime 2026-09-07 23:41.

**Estado: NO LEIDA.** El dueno la aporto el 2026-09-08 de madrugada con instruccion explicita de
**no actuar sobre ella todavia**, solo dejarla en cola. Se tria cuando termine la cola en curso
(revision cruzada T1+T6 y su cierre) y entonces se decide que se hace con ella.

Nadie ha comparado sus hallazgos contra las 25 fichas P1 del council, contra el dictamen de Grok
del 2026-09-08 ni contra lo que corrigio T1/T6: puede solaparse entero, en parte o nada.
**No asumir que es trabajo nuevo, ni que es trabajo ya hecho.** El primer paso de su triaje es esa
comparacion, no implementar nada.

## Proxima accion

~~Las dos auditorias~~ **HECHAS el 2026-09-07**: council de 3 lanes, triage de las 25 P1 y plan
conjunto arbitrado en `reviews/2026-09-06-council-auditorias/PLAN-CONJUNTO.md`. **Y la advertencia
que llevaba esta seccion era FALSA**: la auditoria grande no va por detras de nada — `main` nunca
se movio de `d61705e`, que es el merge-base exacto de las cuatro ramas.

**Lo siguiente, en este orden:**

1. ~~Mirar el `LF_BTCAtm.json` de produccion~~ **CERRADO el 2026-09-07, y la premisa era falsa**:
   el mod corre en MUCHOS servidores, asi que no existe «el» JSON de produccion que inspeccionar.
   El gate pasa a ser el codigo. Defaults vacios + `LogCatalogHelp()` verbose, verificado en caja
   con los dos controles. Detalle arriba en el LIVE-STATE.
   **Ojo al retomar**: el fuente lleva UN cambio posterior a la PBO desplegada — el comentario de
   `LFPG_BTCConfig.c:18`, que citaba `Paper_Bill_100` como ejemplo. Solo comentario, no cambia
   comportamiento, pero fuente y PBO ya no son identicos: el proximo build lo absorbe.
2. **Firmar como decision de producto** los dos residuos de T1: la ventana de duplicacion de E04
   (acotada con marcador durable, ya no es de un solo fallo) y los tombstones que E16 deja para
   ATMs que no vuelven.
3. **Commitear T1** y el guard de `tools/build_guarded.py` (repo distinto).
4. **T2 — autoridad de servidor**: SEC01, SEC02, SEC03, SEC20. Sube por haber jugadores.
5. **T0a — extraccion de la paleta** (`LFPG_ColorData` + 29 `COL_*` fuera de `LFPG_SorterView.c`,
   repuntando las 76 refs del ATM). Prerrequisito de todo lo demas del sorter.

Y despues, por orden de coste:

1. **Los TRES amarres que impiden jubilar V3** — hallados por Grok y **re-verificados por mi**,
   no son opinion:
   - **La cadena RPC de V4 esta abierta por dos puntas.** `SORTER_TEST_RESYNC = 65`
     (`LFPG_Defines.c:331`) tiene ACK despachado (`LFPG_RPCClientHandler.c:79`) pero **ningun
     emisor**: `LFPG_ActionSyncSorter.c:101` emite el 29 de V3. Igual `SORTER_TEST_CARGO_REFRESH
     = 70`, despachado en `:91`, cuyo unico productor `LFPG_NetworkManagerImpl.c:6316` emite el
     34. Borrar los handlers V3 rompe pairing y refresh **en entidades V4, hoy**.
   - **El ATM depende de la UI de V3**: `LFPG_BTCAtmController.c` lee `LFPG_SorterView.COL_RED`,
     `COL_GREEN`, `COL_RED_BTN`, `COL_BTN`, `COL_TEXT_DIM`.
   - **Dos guards se olvidan del TEST**: `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`
     preguntan solo `LFPG_SorterView.IsOpen()`.
   Y una cuarta, de producto: la accion de abrir V4 es un **superconjunto deliberado** de V3
   (abre sin corriente y sin link, por D2). Jubilar V3 sin reponer esos guards cambia el
   producto. La auditoria `muse-spark` senala lo mismo por su cuenta.
2. **La corrida de los 50 casos** — `round-s3b/TEST-PLAN-SORTER-V4.md`. Buena parte saldra
   INCONCLUSO: necesitan F-POWERED y no hay verbo MCP para cablear el grafo electrico.
3. **Que se envian los DOS sorters.** V4 es hoy peso anadido: jubilar la UI de V3 saca
   **201.373 B** (146.795 de scripts en 5 clases + 54.578 de layouts) mas 5.560 del envoltorio
   TEST. Los layouts bajan el PBO directo; los scripts quitan **estructura compilada**, la unica
   palanca que rindio — pero la invariante dice que cortar lineas no reduce el arena: **se mide,
   no se promete**.
4. **Benchmark E/F sobre la RC** — sigue pendiente.
5. **Arranque diag del servidor** — sigue pendiente: primera compilacion real de las 902
   lineas de la deuda v3.
6. **Commitear el guard de `tools/build_guarded.py`** (ese arbol es otro repo git).
4. **G2B y G9B** siguen sin atestar: necesitan tu ojo comparando
   `round-s3b/capture_20260904_141700_320.jpg` contra `screenshot_v3.png`.

Fichas: `fb-20260830-112422-2762` **RESUELTA** — los verbos UI ya devuelven `ui_request`
con `requested_path`/`requested_text` y ademas `matched_path` (la ruta que eligio el
resolver, que es justo lo que hacia falta para diagnosticar colisiones). Siguen abiertas
`fb-20260830-112438-40e4` (`entities_query` no expone cargo) y `fb-20260830-112522-1082`.

## Invariantes CERRADAS — NO retocar / NO re-bakear

- **El hueco de huella es `885 kB`, no `355 kB`** (2026-08-23 · `assumptions.md:15`). El
  `355` de `bug-ledger.md:1123` esta **[RETRACTADO]**: se midio sobre un PBO sellado
  *irrepetible* y **no promovido**. Si vuelve a aparecer, es que se leyo el bug-ledger sin
  cruzar `assumptions.md`.
- **Recortar lineas NO reduce el arena** (2026-07-25/28 · `assumptions.md`). 6.466 lineas
  fuera = `1 kB`; 1.071.376 bytes de fuente fuera = `−2/0 kB`. Solo la reduccion de
  **estructura compilada** rindio (A2, `44/45 kB`). No reabrir esta via.
- **Las fachadas `4_World` + impl `5_Mission` son el mecanismo de las palancas, no ceremonia**
  (`LFPG_RPCServerHandler.c:4-8`). Sacaron 15 clases de `4_World` (199→185). No deshacer.
- **La ventana deslizante del rate limiter cuenta intentos a proposito**
  (`LFPG_NetworkManagerImpl.c:755-759`). Es el unico bloqueo duro anti-spam. No borrar.
- **Las subclases vacias de accion son el mecanismo de registro por typename**
  (`LFPG_ActionRegistration.c:49`). No borrar `LFPG_ActionPlaceLogicGate`.
- **`allowUnpowered` NO llega a ningun build publicado**; nunca cherry-pickear `7e86ca8` solo.

## Punteros (detalle)

- `30_Sessions/2026-09-01-lfpowergrid-auditoria-sobreingenieria-kimi.md` (ultimo)
- `30_Sessions/2026-08-30-*` (ledger de puertas S3-B) · `10_Projects/LF_PowerGrid/bug-ledger.md`
- `assumptions.md` · `validation-matrix.md` · `decisions/decision-log.md` ·
  `verified-apis.md` seccion Contrato del puente MCP y de la sonda TEST (2026-08-30):
  `ui.nodes[]` sin campo `node`, `MCPResult` sin `path`/`text`, predicado de link por cargo,
  y la sonda S1Probe imprimiendo UN CAMPO POR LINEA. Los cuatro deciden si el ledger lee bien
- `20_Knowledge/skill-patches-pending.md` **SP-375** pendiente: append a `dayz-test-ingame`
  (su gate post-build comprueba existencia, no frescura). Aprobado para redaccion, NO aplicado
- Ramas: `sorter/v4-finish` `c4ac943` · `debt/v3-data-integrity` `5df9f47` ·
  `maint/audit-kimi-followup` `1d178ec` · RC `289592b`

**Gate de arranque:** declarar `Retomo LFPowerGrid desde: los 4 tramos commiteados
(T1 34499a1, T2 archivado 8039fde, T0a 17aac47, T5 1f12ac0), con servidor privado con jugadores ·
proxima accion: cerrar el gate de compilacion, que NO se ha corrido nunca para T2, T0a ni T5 --
la PBO de T2 ya esta construida en _staging/@LFPowerGrid_t2r3_20260907/ y solo falta desplegar,
arrancar y mirar Can't compile / CParser, comprobando que el arranque es POSTERIOR al despliegue`
y verificar la fecha de este snapshot antes de actuar. **NO repetir la ventana para re-medir lo ya medido**: los numeros
estan en `round-s3b/RESULTS.md` y en el handoff del 2026-09-06.

**Y antes de fiarte de CUALQUIER medida in-game anterior al 2026-09-06**: pregunta de que PBO
salio. El desplegado hasta esa fecha llevaba fuentes del 29-ago, asi que toda medida sobre el
sorter V4 tomada entre el 30-ago y el 06-sep describe codigo que **no es** el del repo. Las del
2026-09-04 (`round-s3b/RESULTS.md`) salieron del PBO `08FFCD14E320F5EA`, cuya procedencia **no
esta verificada**: no se ha comprobado de que arbol de fuentes se construyo. Eso no las invalida
por si solo, pero tampoco las acredita.

**Tres avisos de metodo que salieron de esta ventana** (detalle en RESULTS.md §1, §4, §5, y
promovidos a la skill `dayz-ui-development`):
- `entities_query` **no ajusta a superficie**: con `y=0` mide una esfera bajo tierra y devuelve
  0. El plan lo ordenaba asi; seguirlo al pie de la letra habria hecho spawnear una fixture
  duplicada. Usar la `y` real (aqui ~36,4).
- **Redimensionar la ventana NO prueba independencia de resolucion**: el motor conserva la
  resolucion de `-x/-y` y ESCALA la superficie (todo por 1280/1920 exacto, hasta un widget de
  1 px). Para otra resolucion, relanzar. Y ojo: la ausencia de `UIScaler`/`ScaleWidget` en el
  mod no prueba que no haya escalado, porque lo hace el motor.
- Un helper host **no DPI-aware** miente al 150% de escalado, y el gate `client_rect==pedido`
  sale VERDE comparando el espacio equivocado. `SetProcessDpiAwareness(2)` antes de medir.
<!-- LIVE-STATE:END -->
