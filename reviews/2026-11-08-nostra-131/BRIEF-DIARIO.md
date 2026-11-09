# BRIEF-DIARIO v2 — diario de cables para apagados sucios (1.3.1)

v2 del 2026-11-09. Sustituye a la v1, que la revision de plan de Codex
(`REVIEW-codex-plan-diario.md`) declaro NO-APTO. La tabla del final dice como se cierra cada hallazgo.
Rama `claude/project-thread-0iolln`, encima de lo que haya. Lee `AGENTS.md` entero; si algo de aqui lo
contradice, manda este brief y dilo en el informe.

## Contexto

Medido el 2026-10-26 (`C:\tmp\lfpg-cables-reinicio\INFORME-2.md`): si el servidor muere sin apagado
limpio antes de que la CE termine de guardar, se pierden los cables hechos, cortados o rerouteados
desde el ultimo guardado. Los cables de un aparato LFPG viajan en su registro CE (`OnStoreSave`) y no
hay funcion de script para forzar un guardado. Guillermo decidio el 2026-11-08 meter en la 1.3.1 un
diario en el perfil del servidor que se reaplica al arrancar.

No cubre: aparatos colocados despues del ultimo guardado (DayZ los pierde enteros; los cables hacia
ellos se podan como hoy), estados de interruptores, cables de aparatos vanilla (`vanilla_wires.json`),
cambios en los 5 s antes de la muerte (el diario escribe cada 5 s).

## Modelo de guardado en el que se apoya

- La CE guarda en pasadas (13-15 s cada ~53-70 s en `lfpg_grafo`, `INFORME-2.md`). En una pasada llama
  a `OnStoreSave` de cada entidad como mucho una vez, y una pasada no empieza hasta que acaba la anterior.
- `OnStoreSave(X)` no hace recuperable a X: si el servidor muere a mitad de pasada, la CE puede cargar X
  o lo de la pasada anterior (WJ-01).
- Por eso, lo que la CE puede cargar de un dueno es uno de sus dos ultimos estados distintos, cargados o
  emitidos: **ultimo** y **anterior**. Si carga otra cosa, el diario no lo esperaba y no toca nada.
- Si la CE no reemitiera una entidad que no cambia, el diseno sigue siendo seguro: solo limpia mas tarde
  (TTL). J11 y J12 miden en partida que una muerte a mitad de pasada carga ultimo o anterior.

## Diseno

1. Cada dueno nativo (`LFPG_WireOwnerBase`, `LFPG_Generator`) recuerda **ultimo** y **anterior**. Al
   cargar: ultimo = lo leido, anterior = "". Al emitir J en `OnStoreSave`: si J != ultimo, anterior =
   ultimo y ultimo = J; si es igual, nada.
2. Entrada por dueno con cambios: `m_WiresJSON` (estado vivo), `m_SavedJSON` (ultimo en la captura) y
   `m_SavedPrevJSON` (anterior en la captura).
3. Arranque, con L = lo que la CE ha cargado:
   - L == `m_WiresJSON`: UNCHANGED, se borra la entrada;
   - L == `m_SavedJSON` o L == `m_SavedPrevJSON`: RESTORE de `m_WiresJSON`;
   - otra cosa: STALE, no se toca, se borra y se avisa.
   Una cadena vacia nunca coincide y nunca se restaura (WJ-04).
4. Retirada, **nunca por un `OnStoreSave` distinto** (WJ-02):
   - emitir `m_WiresJSON` dos veces con al menos `LFPG_WIRE_JOURNAL_PASS_GAP_MS` entre medias prueba que
     la primera pasada termino: la CE ya lo tiene y la entrada se borra (DURABLE);
   - emitir otra cosa sin estar marcado (p. ej. net ids refrescados) marca para recapturar;
   - TTL de respaldo: una entrada sin recaptura en `LFPG_WIRE_JOURNAL_TTL_MS` se borra.
5. Se marca siempre, tambien durante una validacion; se captura solo sin validacion activa (WJ-03).
6. Coste: marcar un id en un mapa al publicar; un Tick cada 5 s que escribe solo si cambio una entrada;
   en cada `OnStoreSave`, una comparacion de cadenas en el dueno y una busqueda en un mapa casi siempre
   vacio. Lo que se escribe en la CE no cambia ni un byte.

## D1 — Ajuste (`scripts/3_Game/LFPG_Settings.c`)

`bool WireJournal = true;` en `LFPG_ServerSettings`, con comentario como los demas que diga tambien:
"Restoring a storage backup: delete wire_journal.json first, or set this to false for that boot".
` WireJournal=<valor>` en la linea `Settings loaded:`, junto a `NotifyWireReplaced=`. Sin cambios en
`ver`, `ValidateAndClamp` ni `Save()`.

## D2 — Datos, constantes y fichero (3_Game)

- `LFPG_Data.c`, junto a `LFPG_VanillaWireStore`:
  - `class LFPG_WireJournalEntry` con exactamente `string m_OwnerDeviceId; string m_WiresJSON;
    string m_SavedJSON; string m_SavedPrevJSON;` (constructor a `""`).
  - `class LFPG_WireJournalStore` con `int ver; ref array<ref LFPG_WireJournalEntry> entries;`
    (constructor: `ver = LFPG_WIRE_JOURNAL_VER`, `entries` nuevo).
  - `class LFPG_WireJournalVersionProbe` con solo `int ver;` (constructor a 0), para leer la version
    sin cargar las entradas.
  - Nada de estado de sesion en estas clases.
- `LFPG_Defines.c`: `LFPG_WIRE_JOURNAL_VER = 1`, `LFPG_WIRE_JOURNAL_MAX_ENTRIES = 128`,
  `LFPG_WIRE_JOURNAL_PASS_GAP_MS = 20000`, `LFPG_WIRE_JOURNAL_TTL_MS = 3600000`,
  `LFPG_WIRE_JOURNAL_DIR = "$profile:LF_PowerGrid"`,
  `LFPG_WIRE_JOURNAL_FILE = "$profile:LF_PowerGrid\\wire_journal.json"`, y los veredictos
  `LFPG_WJ_CAPTURE_REMOVE = 0`, `LFPG_WJ_CAPTURE_KEEP = 1`, `LFPG_WJ_CAPTURE_STORE = 2`,
  `LFPG_WJ_SAVE_IGNORE = 0`, `LFPG_WJ_SAVE_RECAPTURE = 1`, `LFPG_WJ_SAVE_FIRST_EMIT = 2`,
  `LFPG_WJ_SAVE_WAIT = 3`, `LFPG_WJ_SAVE_DURABLE = 4`, `LFPG_WJ_REPLAY_MISSING = 0`,
  `LFPG_WJ_REPLAY_UNUSABLE = 1`, `LFPG_WJ_REPLAY_UNCHANGED = 2`, `LFPG_WJ_REPLAY_RESTORE = 3`,
  `LFPG_WJ_REPLAY_STALE = 4`.
- `LFPG_FileUtil.c`: solo `static bool AtomicSaveWireJournal(string targetPath, LFPG_WireJournalStore store)`,
  copia tipada de `AtomicSaveVanillaWires` (`.tmp`, read-back, `.bak`), y `RECOVERY_WIRE_JOURNAL = 4`
  con su rama en `IsRecoveryFileReadable` solo si ese guardado la necesita. **No** se copia
  `EnsureVanillaWiresFileOrRestore` (WJ-05): la carga la hace el gestor (D4.4) sin promover ni renombrar
  nada. Si ese helper tiene el mismo problema con ficheros de version futura, va al informe, no se toca.

Formato: `{"ver":1,"entries":[{"m_OwnerDeviceId":"<id>","m_WiresJSON":"...","m_SavedJSON":"...","m_SavedPrevJSON":"..."}]}`

## D3 — Ultimo y anterior de cada dueno (4_World)

- `LFPG_WireOwnerBase.c`: `protected string m_LastStoredWireJSON;` y `protected string m_PrevStoredWireJSON;`
  (a `""` en el constructor).
  - `LFPG_OnStoreLoadExtra`: rama de lectura truncada, los dos a `""`; lectura buena,
    `m_LastStoredWireJSON = json;` (tal cual se leyo) y `m_PrevStoredWireJSON = "";`.
  - `LFPG_OnStoreSaveExtra`, despues de calcular `json` en cualquiera de las dos ramas y antes de
    `ctx.Write(json)`: `if (json != m_LastStoredWireJSON) { m_PrevStoredWireJSON = m_LastStoredWireJSON;
    m_LastStoredWireJSON = json; }` y, solo si no esta congelado y dentro de `#ifdef SERVER`,
    `LFPG_WireJournal.Get().OnOwnerSaved(m_DeviceId, json);`.
  - Getters publicos `string LFPG_GetLastStoredWiresJSON()`, `string LFPG_GetPrevStoredWiresJSON()` y
    `bool LFPG_IsWireStoreFrozen()`.
- `LFPG_TestDevices.c`, `LFPG_Generator`: los mismos dos miembros y getters. `OnStoreLoad`, tras leer
  `json` con exito: last = json, prev = "". `OnStoreSave`, tras `SerializeJSON`: la misma regla de
  desplazamiento y la misma llamada a `OnOwnerSaved` bajo `#ifdef SERVER`.
- Los bytes que se escriben en `ctx` son los mismos que antes.

## D4 — Gestor nuevo: `scripts/4_World/LFPG_WireJournal.c` (LF)

Singleton `static ref LFPG_WireJournal s_Instance;` + `static LFPG_WireJournal Get()`. La clase no va
dentro de `#ifdef SERVER`; los cuerpos que tocan ficheros o el mundo, si. Estado de sesion:
`ref map<string, ref LFPG_WireJournalEntry> m_Entries`, `ref map<string, bool> m_Dirty`,
`ref map<string, int> m_NewestEmitMs`, `ref map<string, int> m_CapturedMs`, `ref array<string> m_Restored`,
`bool m_Loaded`, `bool m_Enabled`, `bool m_ReadOnly`, `bool m_FileDirty`, `bool m_CapWarned`,
`int m_SaveFailures`, `int m_LastSaveWarnMs`, `int m_LastKeepWarnMs`. **Activo** = `m_Loaded && m_Enabled && !m_ReadOnly`.

**Usable** (objeto) = `LFPG_DeviceAPI.HasWireStore(obj)`, id no `IsAmbiguous` y, si es
`LFPG_WireOwnerBase`, no congelado. **Ultimo/anterior** del objeto = los getters de D3 por cast a
`LFPG_WireOwnerBase` o a `LFPG_Generator`.

Funciones puras (static, firma en una sola linea, solo escalares):

```
static bool LFPG_WireJournalMarkAllowed(bool active, bool hasEntry, int entryCount, int maxEntries)
    // false si !active; true si hasEntry; false si entryCount >= maxEntries; true en otro caso.
static int LFPG_WireJournalCaptureKind(bool usable, bool liveEmpty, bool savedEmpty, bool liveEqualsSaved, bool prevEmpty)
    // REMOVE si !usable; KEEP si liveEmpty; REMOVE si savedEmpty (la CE aun no tiene el aparato);
    // REMOVE si liveEqualsSaved && prevEmpty (la CE ya tiene ese estado); STORE en otro caso.
static int LFPG_WireJournalSaveVerdict(bool hasEntry, bool isDirty, bool savedEqualsNewest, int newestEmitMs, int nowMs, int passGapMs)
    // IGNORE si !hasEntry o isDirty; RECAPTURE si !savedEqualsNewest; FIRST_EMIT si newestEmitMs < 0;
    // DURABLE si nowMs - newestEmitMs >= passGapMs; WAIT en otro caso.
static bool LFPG_WireJournalTtlExpired(int capturedMs, int nowMs, int ttlMs)
    // false si capturedMs < 0; true si nowMs - capturedMs >= ttlMs; false en otro caso.
static int LFPG_WireJournalReplayVerdict(bool ownerFound, bool usable, bool storedEqualsNewest, bool storedEqualsSaved, bool storedEqualsPrev)
    // MISSING si !ownerFound; UNUSABLE si !usable; UNCHANGED si storedEqualsNewest;
    // RESTORE si storedEqualsSaved o storedEqualsPrev; STALE en otro caso.
```

Metodos:

1. `void MarkOwnerDirty(EntityAI owner)`: id con `LFPG_DeviceAPI.GetDeviceId`; si es `""` o no hay
   almacen, nada. Si `LFPG_WireJournalMarkAllowed(activo, m_Entries.Contains(id), m_Entries.Count(),
   LFPG_WIRE_JOURNAL_MAX_ENTRIES)`, `m_Dirty[id] = true`. Si esta activo y falla solo por el tope, un
   `Warn` por sesion: `[WireJournal] Entry cap <N> reached; new owners are not journaled until entries
   retire`. No mira validacion ni arranque.
2. `void OnOwnerSaved(string ownerId, string savedJSON)`: si no esta activo, nada.
   `v = LFPG_WireJournalSaveVerdict(hayEntrada, m_Dirty.Contains(id), hayEntrada && savedJSON ==
   entrada.m_WiresJSON, emitMs, g_Game.GetTime(), LFPG_WIRE_JOURNAL_PASS_GAP_MS)`, con emitMs =
   `m_NewestEmitMs[id]` o -1. RECAPTURE: `m_Dirty[id] = true` (`Debug [WireJournal] recapture <id>`).
   FIRST_EMIT: `m_NewestEmitMs[id] = ahora`. DURABLE: borra la entrada y sus mapas, `m_FileDirty = true`
   (`Debug [WireJournal] durable <id>`). IGNORE y WAIT: nada.
3. `void Tick(int nowMs)`: **lo primero**, si no esta activo, return.
   a) Si `nm` existe y `!nm.IsValidationActive()`, por cada id de `m_Dirty`: obj con `FindById`; con
      live = `GetWiresJSON(obj)`, saved = ultimo, prev = anterior (los tres `""` si no hay obj),
      `kind = LFPG_WireJournalCaptureKind(obj && usable, live == "", saved == "", live == saved, prev == "")`.
      REMOVE: borra la entrada y sus mapas si la habia (`m_FileDirty`). KEEP: no toca la entrada y
      avisa como mucho cada 60 s (`[WireJournal] Wires of <id> did not serialize; entry kept`). STORE:
      crea (solo si hay sitio bajo el tope) o actualiza la entrada con live, saved y prev; si
      `m_WiresJSON` cambia, `m_NewestEmitMs[id] = -1`; `m_CapturedMs[id] = nowMs`; `m_FileDirty` si
      cambio algun campo. Vacia `m_Dirty` al terminar. Con validacion activa no captura ni vacia nada.
   b) TTL: junta los ids con `LFPG_WireJournalTtlExpired(m_CapturedMs[id], nowMs, LFPG_WIRE_JOURNAL_TTL_MS)`
      y borralos despues del recorrido (`m_FileDirty`).
   c) Si `m_FileDirty`: `MakeDirectory` si falta y `AtomicSaveWireJournal`. Bien: `m_FileDirty = false`.
      Mal: sigue sucio y avisa con el mismo limite de 60 s que `FlushVanillaIfDirty`. Una linea `Debug`
      por guardado: `[WireJournal] saved entries=<n> ms=<t>`.
4. `void ReplayAtBoot()`: si `m_Loaded`, return (idempotente); `m_Loaded = true`; `m_Enabled` = ajuste
   `WireJournal` (true sin ajustes).
   - Lectura sin promover nada (WJ-05): sondea la version de `wire_journal.json` con
     `LFPG_WireJournalVersionProbe` (`JsonFileLoader<...>.LoadFile`, como la carga de
     `vanilla_wires.json`); si no existe o no se lee, sondea `wire_journal.json.bak`. Nunca lee `.tmp`
     ni `.bak.new`, y al cargar no renombra, copia ni borra.
   - Si un fichero legible tiene `ver > LFPG_WIRE_JOURNAL_VER`: `m_ReadOnly = true`, `Warn`
     (`[WireJournal] journal schema v<x> > v<y>; ignored and left untouched`) y return. Vale tambien con
     el ajuste desactivado: no se borra.
   - Desactivado: borra `wire_journal.json`, `.tmp`, `.bak` y `.bak.new` si existen, `Info` si habia
     alguno (`[WireJournal] Disabled by settings; journal files removed`) y return.
   - Carga el primero legible (principal; si no, `.bak`) en un `LFPG_WireJournalStore`. Si fue el
     `.bak`: `Warn` y `m_FileDirty = true`. Si existe alguno y ninguno se lee: `Warn` y diario vacio.
   - Por cada entrada con id no vacio: obj, L = ultimo del obj; usable = usable(obj) && L != "" &&
     `m_WiresJSON != ""`; veredicto `LFPG_WireJournalReplayVerdict(obj != null, usable, L == m_WiresJSON,
     m_SavedJSON != "" && L == m_SavedJSON, m_SavedPrevJSON != "" && L == m_SavedPrevJSON)`.
     - RESTORE: `DeserializeJSON` sobre un **array local nuevo** (nunca sobre el del aparato). Si
       devuelve false, cuenta como UNUSABLE. Si no, `LFPG_DeviceAPI.ClearDeviceWires(obj)` y
       `AddDeviceWire` de cada cable, contando rechazados. La entrada se queda, con
       `m_NewestEmitMs = -1` y `m_CapturedMs = ahora`, y el id va a `m_Restored`.
       `Info [WireJournal] Restored <id> wires=<n> rejected=<k>`.
     - STALE: `Warn [WireJournal] Skipped <id>: stored wires match no journal state`; se borra.
     - MISSING, UNUSABLE, UNCHANGED: se borra (`Debug`).
   - Resumen a `Info`: `[WireJournal] Replay entries=<n> restored=<r> unchanged=<u> stale=<s>
     missing=<m> unusable=<x>`. Si se borro algo, `m_FileDirty = true`.
   - No toques indice inverso, cuentas por jugador ni grafo: las fases 9 y 13 los rehacen.
5. `void OnStartupValidationDone()`: cada id de `m_Restored` a `m_Dirty` y vacia `m_Restored`.

## D5 — Enganches en `scripts/5_Mission/LFPG_NetworkManagerImpl.c` (CRLF)

Igual que la v1:
1. `LFPG_WireJournal.Get().MarkOwnerDirty(owner);` como primera sentencia tras la guarda de `owner`
   nulo de `BroadcastOwnerWires`, `BroadcastOwnerWireDelta`, `QueueBroadcastOwnerSnapshot` y
   `QueueBroadcastOwnerSnapshotFromWires`. Comprueba con `path:line` que los CutAll de
   `LFPG_DeviceLifecycle.c` (`:46`, `:66`, `:114`, `:116`, `:133`) acaban en una de las cuatro, para el
   dueno y para los que apuntaban a el; si alguno no llega, para y avisame. El full sync de un jugador
   no pasa por ellas (`LFPG_SendFullSyncOwner` envia directo), asi que conectar no marca nada.
2. `ValidateAllWiresAndPropagate()`, rama de servidor, tras el bloque `if (m_ValidationActive) {...}`:
   `if (!m_StartupValidationDone) LFPG_WireJournal.Get().ReplayAtBoot();`
3. `LFPG_ValidationFinalize()`: dentro de `if (wasStartupPending)` (con llaves),
   `LFPG_WireJournal.Get().OnStartupValidationDone();`.
4. `LFPG_ServerSchedulerTick()`: en el bloque `m_SchedFlushMs >= LFPG_VANILLA_FLUSH_S * 1000`, despues de
   `FlushVanillaIfDirty();`, `LFPG_WireJournal.Get().Tick(g_Game.GetTime());`.

Nada mas en ese fichero.

## Fuera de alcance

Formato CE, `vanilla_wires.json`, grafo, handlers de RPC, `config.cpp` (comprueba solo que
`worldScriptModule` carga 4_World). Nada adyacente se arregla: va al informe. Ya vistos para el
bug-ledger: `LFPG_Generator.OnStoreLoad` ignora el false de `DeserializeJSON`; y, si se confirma,
`EnsureVanillaWiresFileOrRestore` con ficheros de version futura.

## Tests offline (R26) — antes o junto al codigo, y tienen que morder

En `.github/tools/`, estilo `test_finish_wiring_quota.py`, registrados en `checks.yml`, salida 0 en verde
y distinta de 0 en rojo.

1. `test_wire_journal_rules.py`, las cinco funciones puras del fuente (N = 128, G = 20000, T = 3600000):
   - MarkAllowed: `(true,false,0,N)` true; `(true,true,N,N)` true; `(true,false,N-1,N)` true;
     `(false,true,0,N)` false; `(true,false,N,N)` false. Mutante sin el true por `hasEntry`: el segundo falla.
   - CaptureKind: `(false,…)` REMOVE; `(true,true,…)` KEEP; `(true,false,true,false,true)` REMOVE;
     `(true,false,false,true,true)` REMOVE; `(true,false,false,true,false)` STORE;
     `(true,false,false,false,true)` STORE. Mutante sin la regla `liveEqualsSaved && prevEmpty`: el
     cuarto da STORE y el test lo caza.
   - SaveVerdict: `(false,…)` IGNORE; `(true,true,…)` IGNORE; `(true,false,false,…)` RECAPTURE;
     `(true,false,true,-1,5000,G)` FIRST_EMIT; `(true,false,true,1000,1000+G,G)` DURABLE;
     `(true,false,true,1000,999+G,G)` WAIT. Mutantes: RECAPTURE cambiado por DURABLE (borrar por un
     guardado distinto, como la v1) y `>` en vez de `>=`: los dos se cazan.
   - TtlExpired: `(-1,10^9,T)` false; `(0,T-1,T)` false; `(0,T,T)` true.
   - ReplayVerdict: `(false,…)` MISSING; `(true,false,…)` UNUSABLE; `(true,true,true,true,false)` y
     `(true,true,true,false,false)` UNCHANGED; `(true,true,false,true,false)` RESTORE;
     `(true,true,false,false,true)` RESTORE; `(true,true,false,false,false)` STALE. Mutantes: sin la
     comparacion con `storedEqualsPrev`, y mirando `storedEqualsSaved` antes que `storedEqualsNewest`.
2. `test_wire_journal_scenarios.py`: compone las cinco funciones cargadas del fuente con un modelo
   pequeno en Python del dueno (la regla de desplazamiento de D3) y de los mapas del gestor (D4.2-D4.4),
   y corre estas secuencias; cada una acaba con los cables esperados tras el "arranque":
   - S1 (WJ-01): carga A; cambio a B; `OnStoreSave(B)` antes del Tick; Tick; muerte. CE carga A: queda B.
     CE carga B: queda B (UNCHANGED).
   - S2: carga A; cambio B; Tick; `OnStoreSave(B)`; cambio C; Tick; muerte. CE A o CE B: queda C.
   - S3 (WJ-02): arranque con entrada (B; A; "") y CE A: RESTORE B; la validacion lo deja en B';
     `OnStoreSave(B')` antes del Tick (RECAPTURE); Tick; muerte. CE A o CE B': queda B'.
   - S4: carga A; cambio B; Tick; `OnStoreSave(B)` en t y en t+60 s: la entrada desaparece (DURABLE);
     muerte con CE B: queda B.
   - S5: entrada (B; A; "") y CE X: STALE, queda X.
   - S6: live `""` en el Tick: KEEP, la entrada anterior no cambia; el arranque restaura la anterior.
   - S7: aparato colocado en la sesion (ultimo `""`), con cambios: no hay entrada.
   - S8 (WJ-03): cambio marcado con validacion activa: ningun Tick captura hasta que acaba; el
     siguiente si.
   Mutante de v1 (ReplayVerdict sin `storedEqualsPrev` y SaveVerdict con DURABLE en vez de RECAPTURE):
   S1, S2 o S3 tienen que fallar.
3. `test_wire_journal_structure.py`, sobre los fuentes:
   - Ajuste y linea `Settings loaded:`; las tres clases de datos con exactamente esos campos;
     `AtomicSaveWireJournal` y ningun `EnsureWireJournalFileOrRestore`.
   - Los cuatro enganches de D5.1 antes de la primera llamada a `CoalescePendingOwnerSnapshot(`,
     `StorePendingOwnerSnapshot(`, `CaptureOwnerBroadcastSnapshot(` o `.Send(` de su cuerpo; la unica
     llamada a `ReplayAtBoot(` fuera de su definicion, en `ValidateAllWiresAndPropagate` tras
     `!m_StartupValidationDone`; `OnStartupValidationDone(` dentro de `if (wasStartupPending)`; `Tick(`
     en el bloque de `FlushVanillaIfDirty();`.
   - En los dos duenos: la regla de desplazamiento en los guardados (las dos ramas de
     `LFPG_OnStoreSaveExtra` y el Generator), las asignaciones de las cargas, `OnOwnerSaved(` en los dos
     guardados (el de `LFPG_WireOwnerBase` con la guarda de congelado). La lista de `ctx.Write(` de
     `LFPG_OnStoreSaveExtra` (solo `json`) y de `LFPG_Generator.OnStoreSave` (`m_DeviceIdLow`,
     `m_DeviceIdHigh`, `m_SourceOn`, `json`), en ese orden.
   - En el gestor: el primer argumento de cada `DeserializeJSON(` es un local `new array<ref LFPG_WireData>`;
     `Tick` hace return por inactivo antes de cualquier `AtomicSaveWireJournal(`; `ReplayAtBoot` hace
     return si `m_Loaded` antes de leer nada; `ReplayAtBoot` no lee `.tmp` ni `.bak.new` (solo los borra
     en la rama de desactivado); `MarkOwnerDirty` no menciona `IsValidationActive` y `Tick` lo mira antes
     de capturar. Convenciones: ni `+=`, `-=`, `++`, `--`, ternario, `foreach` ni `Print(`; ningun `ref`
     en locales ni parametros.
   - Mutantes: `Tick` sin la guarda de inactivo; deserializar sobre `GetDeviceWires(`; `MarkOwnerDirty`
     con la guarda de validacion. Los tres hacen fallar el test.

## Gates antes de cada commit

Los de `BRIEF.md` (linter con delta de errores 0 y `ui_reconcile` 0 FAIL; barrido de campos no
declarados, incluidos los getters nuevos; convenciones; gate A de EOL: `LFPG_NetworkManagerImpl.c` CRLF,
el resto LF y el fichero nuevo LF; todos los `test_*.py` en verde antes y despues del commit; diff
minimo, cada linea traza a D1-D5 o a los tests).

## Entrega

Commit con "(Grok, ronda N)" (el numero que toque), sin version de modelo, rutas explicitas, y push.
Revision de Codex (R21) del commit congelado, centrada en: que el arranque nunca pise un estado de la CE
que no sea ultimo o anterior; que nada retire una entrada por un `OnStoreSave` distinto; que DURABLE
exija dos emisiones separadas; cadenas vacias; carga sin promover y version futura intacta; marcado
durante validaciones; almacenes congelados; el Generator; bytes de la CE; coste de los enganches. Salida
en `reviews/2026-11-08-nostra-131/REVIEW-codex-diario-r1.md`. Hallazgos con repro o `path:line`, ronda
siguiente. No arranques DayZ para esto: la prueba en partida es `PRUEBA-DIARIO.md`.

## Como cierra la v2 lo que encontro la revision de plan

| Hallazgo | Cierre |
|---|---|
| WJ-01 base adelantada al emitir | Ultimo y anterior en el dueno (D3); RESTORE con cualquiera de los dos (D4.4). Fuera el ack de 5 min. |
| WJ-02 DROP por net ids refrescados | Un guardado distinto solo marca para recapturar (SaveVerdict RECAPTURE); nunca borra. |
| WJ-03 CutAll durante validacion descartado | `MarkOwnerDirty` no mira la validacion; el Tick aplaza la captura. |
| WJ-04 `""` restaura cero cables | `""` nunca se captura (KEEP) ni se restaura (UNUSABLE) ni coincide. |
| WJ-05 recuperacion que pisa version futura | Carga sin promover ni renombrar; version futura intacta tambien con el ajuste desactivado. |
| J8 y J5 de la prueba | Corregidos en `PRUEBA-DIARIO.md` v2. |
| 5 min no demuestra durabilidad | DURABLE por dos emisiones separadas; TTL de 60 min solo de respaldo. |
| Tiron con 512 entradas | Tope 128; J10 mide tamano y ms. |
| Duenos que cargan tarde | Como la v1: MISSING se borra. Si encuentras un dueno con cables que se registre despues de la validacion de arranque, dilo en el informe. |
| Rollback intencionado de backup (ABA) | Documentado en el comentario del ajuste; la retirada por dos pasadas acota la ventana. |
