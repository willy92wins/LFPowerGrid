# BRIEF-DIARIO v3 — diario de cables para apagados sucios (1.3.1)

v3 del 2026-11-09. Sustituye a la v2, que la revision de plan de Codex (`REVIEW-codex-plan-diario-v2.md`)
declaro NO-APTO porque se apoyaba en un modelo de guardado sin medir (D2-01, D2-02) y por tres fallos
propios (D2-03 a D2-05). El modelo esta medido ahora en `SONDA-CE-PASADAS.md`. La tabla del final dice
como se cierra cada hallazgo. Rama `claude/project-thread-0iolln`, encima de lo que haya. Lee `AGENTS.md`
entero; si algo de aqui lo contradice, manda este brief y dilo en el informe.

## Contexto

Medido el 2026-10-26 (`C:\tmp\lfpg-cables-reinicio\INFORME-2.md`) y otra vez en la sonda: si el servidor
muere sin apagado limpio, se pierden los cables hechos, cortados o rerouteados despues de la ultima
rafaga de guardado. Los cables de un aparato LFPG viajan en su registro CE (`OnStoreSave`) y no hay
funcion de script para forzar un guardado. Guillermo decidio el 2026-11-08 meter en la 1.3.1 un diario en
el perfil del servidor que se reaplica al arrancar.

No cubre: aparatos colocados despues del ultimo guardado (DayZ los pierde enteros; los cables hacia ellos
se podan como hoy), estados de interruptores, cables de aparatos vanilla (`vanilla_wires.json`), cambios
en los 5 s antes de la muerte (el diario escribe cada 5 s), ni deshacer un cambio en esa ventana (G5), ni un corte de luz del host (la escritura del
fichero no se confirma a disco).

## Contrato de guardado (medido en `SONDA-CE-PASADAS.md`, mundo `lfpg_grafo`, DayZDiag)

- **C1.** La CE llama a `OnStoreSave` de cada entidad una vez por pasada, todas juntas en una rafaga de
  0-1 s, tambien las que no cambian (18 rafagas de 71 entidades, nunca dos de un dueno en la misma).
- **C2.** Una pasada dura unos 38-39 s en disco y la siguiente empieza unos 14 s despues de su ultima
  escritura (una rafaga cada 52-55 s): no se solapan. Las dos rafagas del arranque (en el segundo de la
  carga y 24-27 s despues) no estan caracterizadas.
- **C3.** Tras una muerte, la CE carga de cada dueno su ultima emision (las 4 muertes de la sonda: antes
  de la rafaga, entre pasadas, con `dynamic` a medias y con `building` a medias; 69 duenos cada vez) o,
  solo en la ventana entre la rafaga y la escritura de su fichero, la anterior (WJ-01: la sonda no llego a
  esa ventana, asi que ni se ha visto ni se descarta). Una generacion mas vieja (`.001`/`.002` por un
  fichero danado) no se ha visto y queda fuera del contrato.
- **C4.** El JSON de un dueno sin cambios es estable entre pasadas, salvo una vez tras arrancar (16 de 16
  cambios en la segunda rafaga; hipotesis: net ids que refresca la validacion).
- Un apagado limpio hace una pasada final con todos.
- Mundo pequeno: un servidor con miles de entidades o retail no esta medido, y la carga de los
  Generators no se comparo (solo su `OnStoreSave`). El informe de la implementacion no lo da por bueno.

## Garantias y limites declarados

- **G1, nunca peor que sin diario fuera de la ventana de G5.** El arranque solo escribe sobre lo que cargo la CE (L)
  si L es uno de los estados guardados con la entrada, y lo que escribe es W, el ultimo estado capturado. Si al
  morir el estado vivo es W, queda W o lo que cargo la CE, nunca otra cosa. Con cualquier otra L (generacion vieja,
  backup restaurado a un estado desconocido) no toca nada: STALE.
- **G2, que recupera.** Un cambio capturado (Tick cada 5 s, sin validacion activa, bajo el tope, con
  serializacion buena) y escrito antes de la muerte vuelve si la CE carga dentro de C3 (el estado con el que arranco
  la sesion cuenta como la emision de antes de la primera). Cuando el dueno ya emitio W dos veces, cargar el estado
  de arranque es una generacion vieja: fuera del contrato.
- **G3, ABA.** Restaurar a mano un backup cuyo estado de un dueno coincida con uno de los guardados
  mientras vive la entrada (hasta DURABLE, unas dos pasadas tras el ultimo cambio, o el TTL) reaplica el
  diario. Lo documenta el comentario del ajuste.
- **G4, retirada antes de tiempo.** Si C1 o C2 fallan en otro servidor (un dueno emitido dos veces en una
  pasada), DURABLE puede retirar pronto: esa muerte se comporta como sin diario, nunca peor.
- **G5, deshacer dentro de la ventana (D3-01).** La ventana de un dueno va de su ultimo cambio al Tick que lo
  escribe: 5 s, mas lo que dure una validacion activa o el reintento de una serializacion fallida. Si en ella el dueno
  vuelve a un estado que la CE puede tener (por ejemplo, se anade un cable y se corta), una muerte dentro de la
  ventana restaura el ultimo estado capturado: reaparece lo deshecho, aunque sin diario se habria quedado bien. Es la
  ventana en la que G2 ya no promete nada, y el jugador lo arregla repitiendo el cambio. Cerrarla exige escribir el
  fichero en cada cambio, que es el coste que el diseno evita.
- Tambien fuera, declarado: cables que cambien entre la carga y el replay (el diario aun no esta activo y
  el RESTORE los pisa; la validacion poda los que apunten a aparatos que faltan) y cambios que no pasen
  por los cuatro enganches de D5.

## Diseno

1. Cada dueno nativo (`LFPG_WireOwnerBase`, `LFPG_Generator`) recuerda **cargado** (lo leido de la CE, no
   cambia en la sesion), **ultimo** y **anterior**. Al cargar: cargado = ultimo = lo leido, anterior = "".
   Al emitir J en `OnStoreSave`: si J != ultimo, anterior = ultimo y ultimo = J; si es igual, nada.
2. Entrada por dueno con cambios: `m_WiresJSON` (estado vivo, W), `m_SavedJSON` (ultimo en la captura),
   `m_SavedPrevJSON` (anterior en la captura) y `m_LoadedJSON` (cargado en la captura).
3. Arranque, con L = **cargado** del dueno (no ultimo: la rafaga de la carga puede emitir antes del
   replay):
   - L == W: UNCHANGED, se borra la entrada;
   - L igual a uno de los otros tres campos (no vacio): RESTORE de W;
   - otra cosa: STALE, no se toca, se borra y se avisa.
   Una cadena vacia nunca coincide y nunca se restaura (WJ-04).
4. Retirada: **DURABLE** cuando el dueno emite W por segunda vez desde que se capturo (por C1 la segunda
   esta en otra pasada y por C2 la primera ya termino). Una emision distinta de W no hace nada. TTL de
   respaldo de 60 min desde la ultima captura. Sin plazo de tiempo entre emisiones.
5. Se marca siempre, tambien durante una validacion; se captura solo sin validacion activa (WJ-03). Un
   fallo de serializacion deja el id marcado para el siguiente Tick (D2-05).
6. El fichero se lee sin promover nada, eligiendo entre los cuatro que deja el guardado atomico, con la
   version de todos mirada antes de cargar o borrar ninguno (D2-03, D2-04).
7. Coste: marcar un id en un mapa al publicar; un Tick cada 5 s que escribe solo si cambio una entrada;
   en cada `OnStoreSave`, una comparacion de cadenas en el dueno y una busqueda en un mapa casi siempre
   vacio. Las emisiones nunca provocan escrituras del fichero salvo una retirada. Lo que se escribe en la
   CE no cambia ni un byte.

## D1 — Ajuste (`scripts/3_Game/LFPG_Settings.c`)

`bool WireJournal = true;` en `LFPG_ServerSettings`, con comentario como los demas que diga tambien:
"Restoring a storage backup: delete wire_journal.json first, or set this to false for that boot".
` WireJournal=<valor>` en la linea `Settings loaded:`, junto a `NotifyWireReplaced=`. Sin cambios en
`ver`, `ValidateAndClamp` ni `Save()`.

## D2 — Datos, constantes y fichero (3_Game)

- `LFPG_Data.c`, junto a `LFPG_VanillaWireStore`:
  - `class LFPG_WireJournalEntry` con exactamente `string m_OwnerDeviceId; string m_WiresJSON;
    string m_SavedJSON; string m_SavedPrevJSON; string m_LoadedJSON;` (constructor a `""`).
  - `class LFPG_WireJournalStore` con `int ver; ref array<ref LFPG_WireJournalEntry> entries;`
    (constructor: `ver = LFPG_WIRE_JOURNAL_VER`, `entries` nuevo).
  - Nada de estado de sesion en estas clases. Sin clase de sondeo de version: se usa
    `LFPG_FileUtil.TryReadRawJsonVersion`, que ya existe (`LFPG_FileUtil.c:865`).
- `LFPG_Defines.c`: `LFPG_WIRE_JOURNAL_VER = 1`, `LFPG_WIRE_JOURNAL_MAX_ENTRIES = 128`,
  `LFPG_WIRE_JOURNAL_TTL_MS = 3600000`, `LFPG_WIRE_JOURNAL_DIR = "$profile:LF_PowerGrid"`,
  `LFPG_WIRE_JOURNAL_FILE = "$profile:LF_PowerGrid\\wire_journal.json"`, y los veredictos
  `LFPG_WJ_CAPTURE_REMOVE = 0`, `LFPG_WJ_CAPTURE_KEEP = 1`, `LFPG_WJ_CAPTURE_STORE = 2`,
  `LFPG_WJ_SAVE_IGNORE = 0`, `LFPG_WJ_SAVE_FIRST_EMIT = 1`, `LFPG_WJ_SAVE_DURABLE = 2`,
  `LFPG_WJ_REPLAY_MISSING = 0`, `LFPG_WJ_REPLAY_UNUSABLE = 1`, `LFPG_WJ_REPLAY_UNCHANGED = 2`,
  `LFPG_WJ_REPLAY_RESTORE = 3`, `LFPG_WJ_REPLAY_STALE = 4`, `LFPG_WJ_FILE_ABSENT = 0`,
  `LFPG_WJ_FILE_FUTURE = 1`, `LFPG_WJ_FILE_CANDIDATE = 2`. Sin `PASS_GAP`.
- `LFPG_FileUtil.c`: solo `static bool AtomicSaveWireJournal(string targetPath, LFPG_WireJournalStore store)`,
  copia tipada de `AtomicSaveVanillaWires` (`.tmp`, read-back, `.bak.new`, `.bak`), sin constante
  `RECOVERY_*` nueva (ese guardado no la usa). **No** se copia `EnsureVanillaWiresFileOrRestore` (WJ-05).

Formato: `{"ver":1,"entries":[{"m_OwnerDeviceId":"<id>","m_WiresJSON":"...","m_SavedJSON":"...","m_SavedPrevJSON":"...","m_LoadedJSON":"..."}]}`

Ficheros que deja `AtomicSaveWireJournal` (`LFPG_FileUtil.c:74-145`) si muere en cada paso, del mas nuevo
al mas viejo: `.tmp` (escrito primero; solo queda si el guardado no termino), principal, `.bak.new` (el
principal anterior, mientras se rota) y `.bak`. Un `.tmp` o `.bak` a medio escribir no carga. Por eso el
orden de lectura de D4.4 es `.tmp`, principal, `.bak.new`, `.bak`.

## D3 — Cargado, ultimo y anterior de cada dueno (4_World)

- `LFPG_WireOwnerBase.c`: `protected string m_LoadedWireJSON;`, `protected string m_LastStoredWireJSON;`
  y `protected string m_PrevStoredWireJSON;` (a `""` en el constructor).
  - `LFPG_OnStoreLoadExtra`: rama de lectura truncada, los tres a `""`; lectura buena,
    `m_LoadedWireJSON = json; m_LastStoredWireJSON = json; m_PrevStoredWireJSON = "";` (tal cual se leyo).
  - `LFPG_OnStoreSaveExtra`, despues de calcular `json` en cualquiera de las dos ramas y antes de
    `ctx.Write(json)`: `if (json != m_LastStoredWireJSON) { m_PrevStoredWireJSON = m_LastStoredWireJSON;
    m_LastStoredWireJSON = json; }` y, solo si no esta congelado y dentro de `#ifdef SERVER`,
    `LFPG_WireJournal.Get().OnOwnerSaved(m_DeviceId, json);`.
  - Getters publicos `string LFPG_GetLoadedWiresJSON()`, `string LFPG_GetLastStoredWiresJSON()`,
    `string LFPG_GetPrevStoredWiresJSON()` y `bool LFPG_IsWireStoreFrozen()`.
  - `m_LoadedWireJSON` solo se asigna en la carga.
- `LFPG_TestDevices.c`, `LFPG_Generator`: los mismos tres miembros y getters. `OnStoreLoad`, tras leer
  `json` con exito: los tres como arriba. `OnStoreSave`, tras `SerializeJSON`: la misma regla de
  desplazamiento y la misma llamada a `OnOwnerSaved` bajo `#ifdef SERVER`.
- Los bytes que se escriben en `ctx` son los mismos que antes.

## D4 — Gestor nuevo: `scripts/4_World/LFPG_WireJournal.c` (LF)

Singleton `static ref LFPG_WireJournal s_Instance;` + `static LFPG_WireJournal Get()`. La clase no va
dentro de `#ifdef SERVER`; los cuerpos que tocan ficheros o el mundo, si. Estado de sesion:
`ref map<string, ref LFPG_WireJournalEntry> m_Entries`, `ref map<string, bool> m_Dirty`,
`ref map<string, bool> m_SeenEmit`, `ref map<string, int> m_CapturedMs`, `ref array<string> m_Restored`,
`bool m_Loaded`, `bool m_Enabled`, `bool m_ReadOnly`, `bool m_FileDirty`, `bool m_CapWarned`,
`int m_LastSaveWarnMs`, `int m_LastKeepWarnMs`. **Activo** = `m_Loaded && m_Enabled && !m_ReadOnly`.

**Usable** (objeto) = `LFPG_DeviceAPI.HasWireStore(obj)`, id no `IsAmbiguous` y, si es
`LFPG_WireOwnerBase`, no congelado. **Cargado/ultimo/anterior** del objeto = los getters de D3 por cast a
`LFPG_WireOwnerBase` o a `LFPG_Generator`.

Funciones puras (static, firma en una sola linea, solo escalares):

```
static bool LFPG_WireJournalMarkAllowed(bool active, bool hasEntry, int entryCount, int maxEntries)
    // false si !active; true si hasEntry; false si entryCount >= maxEntries; true en otro caso.
static int LFPG_WireJournalCaptureKind(bool usable, bool liveEmpty, bool savedEmpty, bool liveEqualsSaved, bool prevEmpty, bool loadedEmpty)
    // REMOVE si !usable; KEEP si liveEmpty; REMOVE si savedEmpty && prevEmpty && loadedEmpty (no se ha
    // leido ni emitido nada: la CE no puede tener el aparato); REMOVE si liveEqualsSaved && prevEmpty;
    // STORE en otro caso. Un ultimo vacio con anterior o cargado presentes es una emision fallida
    // (D3-02): la CE puede tener cualquiera de los dos y la entrada los guarda.
static int LFPG_WireJournalSaveVerdict(bool hasEntry, bool isDirty, bool savedEqualsNewest, bool seenOnce)
    // IGNORE si !hasEntry, isDirty o !savedEqualsNewest; FIRST_EMIT si !seenOnce; DURABLE en otro caso.
static bool LFPG_WireJournalTtlExpired(int capturedMs, int nowMs, int ttlMs)
    // false si capturedMs < 0; true si nowMs - capturedMs >= ttlMs; false en otro caso.
static int LFPG_WireJournalReplayVerdict(bool ownerFound, bool usable, bool storedEqualsNewest, bool storedEqualsSaved, bool storedEqualsPrev, bool storedEqualsLoaded)
    // MISSING si !ownerFound; UNUSABLE si !usable; UNCHANGED si storedEqualsNewest;
    // RESTORE si storedEqualsSaved, storedEqualsPrev o storedEqualsLoaded; STALE en otro caso.
static int LFPG_WireJournalFileVerdict(bool exists, bool versionRead, int version, int currentVer)
    // ABSENT si !exists; FUTURE si versionRead && version > currentVer; CANDIDATE en otro caso.
```

Metodos:

1. `void MarkOwnerDirty(EntityAI owner)`: id con `LFPG_DeviceAPI.GetDeviceId`; si es `""` o no hay
   almacen, nada. Si `LFPG_WireJournalMarkAllowed(activo, m_Entries.Contains(id), m_Entries.Count(),
   LFPG_WIRE_JOURNAL_MAX_ENTRIES)`, `m_Dirty[id] = true`. Si esta activo y falla solo por el tope, un
   `Warn` por sesion: `[WireJournal] Entry cap <N> reached; new owners are not journaled until entries
   retire`. No mira validacion ni arranque.
2. `void OnOwnerSaved(string ownerId, string savedJSON)`: si no esta activo, nada.
   `v = LFPG_WireJournalSaveVerdict(hayEntrada, m_Dirty.Contains(id), hayEntrada && savedJSON ==
   entrada.m_WiresJSON, m_SeenEmit.Contains(id))`. FIRST_EMIT: `m_SeenEmit[id] = true` y
   `Debug [WireJournal] first-emit <id>` (M2-02). DURABLE: borra la entrada y sus mapas,
   `m_FileDirty = true` y `Debug [WireJournal] durable <id>`. IGNORE: nada.
3. `void Tick(int nowMs)`: **lo primero**, si no esta activo, return.
   a) Si `nm` existe y `!nm.IsValidationActive()`, por cada id de `m_Dirty`: obj con `FindById`; con
      live = `GetWiresJSON(obj)`, saved = ultimo, prev = anterior y loaded = cargado (los cuatro `""` si no
      hay obj), `kind = LFPG_WireJournalCaptureKind(obj && usable, live == "", saved == "", live == saved,
      prev == "", loaded == "")`.
      - REMOVE: borra la entrada y sus mapas si la habia (`m_FileDirty`).
      - KEEP: no toca la entrada; **el id sigue marcado** para el siguiente Tick y, si el obj es
        `LFPG_WireOwnerBase`, llama a `LFPG_InvalidateWireJSONCache()` para que el reintento serialice de
        verdad (el getter guarda tambien el fallo, `LFPG_WireOwnerBase.c:166-173`). Avisa como mucho cada
        60 s: `[WireJournal] Wires of <id> did not serialize; retrying`.
      - STORE: crea (solo si hay sitio bajo el tope) o actualiza la entrada con live, saved, prev y
        loaded; si `m_WiresJSON` cambia, quita `m_SeenEmit[id]`; `m_CapturedMs[id] = nowMs`;
        `m_FileDirty` si cambio algun campo.
      Al terminar, en `m_Dirty` solo quedan los KEEP. Con validacion activa no captura ni quita nada.
   b) TTL: junta los ids con `LFPG_WireJournalTtlExpired(m_CapturedMs[id], nowMs, LFPG_WIRE_JOURNAL_TTL_MS)`
      y borralos despues del recorrido (`m_FileDirty`, `Debug [WireJournal] ttl <id>`).
   c) Si `m_FileDirty`: `MakeDirectory` si falta y `AtomicSaveWireJournal`. Bien: `m_FileDirty = false`.
      Mal: sigue sucio y avisa con el mismo limite de 60 s que `FlushVanillaIfDirty`. Una linea `Debug`
      por guardado: `[WireJournal] saved entries=<n> ms=<t>`.
4. `void ReplayAtBoot()`: si `m_Loaded`, return (idempotente); `m_Loaded = true`; `m_Enabled` = ajuste
   `WireJournal` (true sin ajustes). Candidatos, en este orden: `wire_journal.json.tmp`,
   `wire_journal.json`, `wire_journal.json.bak.new`, `wire_journal.json.bak`.
   - **Primero, la version de los cuatro** (D2-04): `LFPG_WireJournalFileVerdict(FileExist(p),
     LFPG_FileUtil.TryReadRawJsonVersion(p, ver), ver, LFPG_WIRE_JOURNAL_VER)`. Si alguno es FUTURE:
     `m_ReadOnly = true`, `Warn` (`[WireJournal] <fichero> has schema v<x> > v<y>; journal off this boot,
     files left untouched`) y return, tambien con el ajuste desactivado. Nada se carga, borra ni escribe.
   - Desactivado: borra los candidatos que existan, `Info` si habia alguno (`[WireJournal] Disabled by
     settings; journal files removed`) y return.
   - Carga el primer candidato, en el orden de arriba, que `JsonFileLoader<LFPG_WireJournalStore>.LoadFile`
     lea entero y con `ver == LFPG_WIRE_JOURNAL_VER` (D2-03). Si no fue el principal: `Warn` con el nombre
     y `m_FileDirty = true`. Si existe alguno y ninguno se lee: `Warn` y diario vacio. Al cargar no
     renombra, copia ni borra nada (WJ-05).
   - Por cada entrada con id no vacio: obj, L = **cargado** del obj; usable = usable(obj) && L != "" &&
     `m_WiresJSON != ""`; veredicto `LFPG_WireJournalReplayVerdict(obj != null, usable, L == m_WiresJSON,
     m_SavedJSON != "" && L == m_SavedJSON, m_SavedPrevJSON != "" && L == m_SavedPrevJSON,
     m_LoadedJSON != "" && L == m_LoadedJSON)`.
     - RESTORE: `DeserializeJSON` sobre un **array local nuevo** (nunca sobre el del aparato). Si
       devuelve false, cuenta como UNUSABLE. Si no, `LFPG_DeviceAPI.ClearDeviceWires(obj)` y
       `AddDeviceWire` de cada cable, contando rechazados. La entrada se queda, sin `m_SeenEmit` y con
       `m_CapturedMs = ahora`, y el id va a `m_Restored`.
       `Info [WireJournal] Restored <id> wires=<n> rejected=<k>`.
     - STALE: `Warn [WireJournal] Skipped <id>: stored wires match no journal state`; se borra.
     - MISSING, UNUSABLE, UNCHANGED: se borra (`Debug`).
   - Resumen a `Info`: `[WireJournal] Replay entries=<n> restored=<r> unchanged=<u> stale=<s>
     missing=<m> unusable=<x>`. Si se borro algo, `m_FileDirty = true`.
   - No toques indice inverso, cuentas por jugador ni grafo: las fases 9 y 13 los rehacen.
5. `void OnStartupValidationDone()`: cada id de `m_Restored` a `m_Dirty` y vacia `m_Restored`.

## D5 — Enganches en `scripts/5_Mission/LFPG_NetworkManagerImpl.c` (CRLF)

Igual que la v2 (la revision de plan v2 comprobo en su punto i que los cinco CutAll llegan a ellos):
1. `LFPG_WireJournal.Get().MarkOwnerDirty(owner);` como primera sentencia tras la guarda de `owner`
   nulo de `BroadcastOwnerWires`, `BroadcastOwnerWireDelta`, `QueueBroadcastOwnerSnapshot` y
   `QueueBroadcastOwnerSnapshotFromWires`. En `QueueBroadcastOwnerSnapshotFromWires`, anadir `if (!owner) return;` y, a
   continuacion, `MarkOwnerDirty(owner)`, antes de `CaptureOwnerBroadcastSnapshot` (M3-01; con owner nulo esa llamada ya
   devuelve null y la funcion no hace nada, asi que no cambia nada de lo que hay). El full sync de un jugador no pasa por ellas
   (`LFPG_SendFullSyncOwner` envia directo), asi que conectar no marca nada.
2. `ValidateAllWiresAndPropagate()`, rama de servidor, tras el bloque `if (m_ValidationActive) {...}`:
   `if (!m_StartupValidationDone) LFPG_WireJournal.Get().ReplayAtBoot();`
3. `LFPG_ValidationFinalize()`: dentro de `if (wasStartupPending)` (con llaves),
   `LFPG_WireJournal.Get().OnStartupValidationDone();`.
4. `LFPG_ServerSchedulerTick()`: en el bloque `m_SchedFlushMs >= LFPG_VANILLA_FLUSH_S * 1000`, despues de
   `FlushVanillaIfDirty();`, `LFPG_WireJournal.Get().Tick(g_Game.GetTime());`.

Nada mas en ese fichero.

## Fuera de alcance

Formato CE, `vanilla_wires.json`, grafo, handlers de RPC, `config.cpp` (comprueba solo que
`worldScriptModule` carga 4_World). Nada adyacente se arregla: va al informe. Para el bug-ledger:
`LFPG_Generator.OnStoreLoad` ignora el false de `DeserializeJSON`; `EnsureVanillaWiresFileOrRestore` con
ficheros de version futura, si se confirma; y `LFPG_WireOwnerBase.LFPG_GetWiresJSON` guarda en cache una
serializacion fallida (`""`), asi que `OnStoreSave` la escribe y al cargar el aparato queda sin cables
(`DeserializeJSON("")` devuelve true, `LFPG_WireHelper.c:335`).

## Tests offline (R26) — antes o junto al codigo, y tienen que morder

En `.github/tools/`, estilo `test_finish_wiring_quota.py`, registrados en `checks.yml`, salida 0 en verde
y distinta de 0 en rojo.

1. `test_wire_journal_rules.py`, las seis funciones puras del fuente (N = 128, T = 3600000):
   - MarkAllowed: `(true,false,0,N)` true; `(true,true,N,N)` true; `(true,false,N-1,N)` true;
     `(false,true,0,N)` false; `(true,false,N,N)` false. Mutante sin el true por `hasEntry`: el segundo falla.
   - CaptureKind (seis argumentos): `(false,…)` REMOVE; `(true,true,…)` KEEP; `(true,false,true,false,true,true)` REMOVE;
     `(true,false,false,true,true,false)` REMOVE; `(true,false,false,true,false,false)` STORE;
     `(true,false,false,false,true,false)` STORE; `(true,false,true,false,false,false)` STORE;
     `(true,false,true,false,true,false)` STORE. Mutante sin la regla `liveEqualsSaved && prevEmpty`:
     `(true,false,false,true,true,false)` da STORE y el test lo caza. Mutante con la regla vieja (REMOVE por `savedEmpty`
     solo): los dos ultimos dan REMOVE y se cazan.
   - SaveVerdict: `(false,…)` IGNORE; `(true,true,…)` IGNORE; `(true,false,false,…)` IGNORE;
     `(true,false,true,false)` FIRST_EMIT; `(true,false,true,true)` DURABLE. Mutantes: DURABLE en la
     primera emision y sin mirar `isDirty`: los dos se cazan.
   - TtlExpired: `(-1,10^9,T)` false; `(0,T-1,T)` false; `(0,T,T)` true.
   - ReplayVerdict: `(false,…)` MISSING; `(true,false,…)` UNUSABLE; `(true,true,true,true,true,true)` y
     `(true,true,true,false,false,false)` UNCHANGED; RESTORE con cada uno de los tres ultimos a true por
     separado; `(true,true,false,false,false,false)` STALE. Mutantes: sin `storedEqualsPrev`, sin
     `storedEqualsLoaded`, y mirando `storedEqualsSaved` antes que `storedEqualsNewest`.
   - FileVerdict: `(false,…)` ABSENT; `(true,true,2,1)` FUTURE; `(true,true,1,1)`, `(true,true,0,1)` y
     `(true,false,0,1)` CANDIDATE. Mutante `>=` en vez de `>`: `(true,true,1,1)` da FUTURE y se caza.
2. `test_wire_journal_scenarios.py`: compone las seis funciones cargadas del fuente con un modelo pequeno
   en Python del dueno (D3) y de los mapas del gestor (D4.2-D4.5), y corre estas secuencias. "Emite" es
   una rafaga (C1), una entrada se escribe (W; ultimo; anterior; cargado), y cada caso termina en muerte
   y arranque con lo que carga la CE; el Tick escribe el fichero:
   - S1 (WJ-01): carga A; cambio B; emite B antes del Tick; Tick. CE A o CE B: queda B.
   - S2: carga A; cambio B; Tick; emite B; cambio C; Tick. CE A o CE B: queda C.
   - S3 (WJ-02): arranque con entrada (B; A; ""; A) y CE A, con la rafaga de la carga emitiendo A: RESTORE
     B; la validacion lo deja en B' sin enganche; emite B' antes de recapturar; fin de validacion; Tick.
     Otra muerte con CE A o CE B': queda B'.
   - S4: carga A; cambio B; Tick; emite B; Tick; emite B; Tick: la entrada ya no esta (DURABLE). CE B:
     queda B.
   - S5: carga A; cambio B; Tick. CE X: STALE, queda X.
   - S6 (D2-05): carga A; cambio B; Tick; cambio C con la serializacion fallando; Tick (KEEP); la
     serializacion vuelve sin mas cambios; Tick. CE A: queda C.
   - S7: aparato colocado en la sesion y nunca emitido, con cambios: no hay entrada; sin el aparato en la
     CE no pasa nada.
   - S8 (WJ-03): carga A; cambio B marcado con validacion activa: ningun Tick captura hasta que acaba; el
     siguiente si. CE A: queda B.
   - S9 (D2-01): carga A; cambio B; emite B; cambio C; emite C; Tick. CE A, CE B o CE C: queda C.
   - S10: carga A; B, C y D emitidos uno por pasada; cambio E; Tick. CE B (generacion vieja): STALE,
     queda B. CE C, CE D o CE A: queda E.
   - S11 (M2-01): aparato colocado en la sesion con estado P; emite P; cambio B; Tick: hay entrada
     (B; P; ""; ""). Sin el aparato en la CE: MISSING. CE P: queda B.
   - S12: carga A; cambio B; Tick; emite B (una sola vez); Tick. CE A: queda B. CE B: queda B.
   - S13: arranque con entrada (B; A; ""; A), CE A y la rafaga de la carga emitiendo A* distinto de A
     antes del replay: RESTORE, queda B.
   - S14 (D3-02): arranque con entrada (B; A; ""; A) y CE A: RESTORE B; fin de validacion; emite con la serializacion
     fallando (ultimo "", anterior A); la serializacion vuelve; Tick: la entrada queda (B; ""; A; A). Otra muerte con CE
     A: queda B.
   - S15: carga A; cambio B; emite con la serializacion fallando; la serializacion vuelve; Tick. CE A: queda B.
   Mutantes, cada uno rompe al menos lo indicado: la regla vieja de `savedEmpty` (S14 y S15); replay sin `storedEqualsPrev` (S9 con CE B, S10 con CE
   C); sin `storedEqualsLoaded` (S9 y S10 con CE A); DURABLE en la primera emision (S12 con CE A); KEEP
   que desmarca el id (S6); captura que guarda ultimo en `m_SavedPrevJSON` (S9 con CE B); replay que
   compara con ultimo en vez de cargado (S13).
   Y una prueba con semilla fija: al menos 10 000 historiales de un dueno con estados repetidos, varias sesiones (muerte,
   arranque y seguir) y fallos de serializacion, con la CE cargando la ultima emision o la anterior (el estado cargado
   cuenta como la emision de antes de la primera; se saltan las cargas de `""`, que es el fallo del bug-ledger de
   arriba). Comprueba dos cosas. (a) Edad de un estado = tiempo desde la ultima vez que estuvo vivo (el vivo al morir
   tiene 0). Si queda un estado mas viejo que el que cargo la CE, el estado vivo al morir no es el W del fichero
   (ventana de G5). (b) Si el ultimo cambio se capturo y se escribio con serializacion buena, queda el estado vivo al
   morir. Con la regla vieja de `savedEmpty`, (b) tiene que fallar.
   - Modelo en Python con varias sesiones, estados repetidos y serializaciones fallidas, unas 258 600 muertes: todo
     resultado mas viejo que lo que cargo la CE cae dentro de la ventana de G5, ninguno fuera; con el arreglo de D3-02,
     ningun caso de G2 sin recuperar (249 con la regla vieja). El test tiene que dar lo mismo con las funciones del
     fuente.
   - O3-01: los escenarios del modelo no ven un despacho mal hecho en el gestor real. Ademas del modelo, ejecuta las
     transiciones reales de `OnOwnerSaved` del fuente (con `enforce_scalar_slice` o un preprocesador en el test, y mapas
     simulados): FIRST_EMIT no borra la entrada, DURABLE la borra. Mutante que intercambia el despacho de FIRST_EMIT y
     DURABLE en `OnOwnerSaved`: tiene que fallar.
3. `test_wire_journal_structure.py`, sobre los fuentes. El modelo de 2 reimplementa el gestor, asi que
   este test fija sus asignaciones reales (O2-03):
   - Ajuste y linea `Settings loaded:`; las dos clases de datos con exactamente esos campos;
     `AtomicSaveWireJournal` y ningun `EnsureWireJournalFileOrRestore` ni `LFPG_WireJournalVersionProbe`;
     ninguna constante `PASS_GAP`.
   - Los cuatro enganches de D5.1 antes de la primera llamada a `CoalescePendingOwnerSnapshot(`,
     `StorePendingOwnerSnapshot(`, `CaptureOwnerBroadcastSnapshot(` o `.Send(` de su cuerpo; la unica
     llamada a `ReplayAtBoot(` fuera de su definicion, en `ValidateAllWiresAndPropagate` tras
     `!m_StartupValidationDone`; `OnStartupValidationDone(` dentro de `if (wasStartupPending)`; `Tick(`
     en el bloque de `FlushVanillaIfDirty();`.
   - En los dos duenos: la regla de desplazamiento en los guardados (las dos ramas de
     `LFPG_OnStoreSaveExtra` y el Generator); `m_LoadedWireJSON` asignado solo en las cargas; las
     asignaciones de las cargas; `OnOwnerSaved(` en los dos guardados (el de `LFPG_WireOwnerBase` con la
     guarda de congelado). La lista de `ctx.Write(` de `LFPG_OnStoreSaveExtra` (solo `json`) y de
     `LFPG_Generator.OnStoreSave` (`m_DeviceIdLow`, `m_DeviceIdHigh`, `m_SourceOn`, `json`), en ese orden.
   - En el gestor: la captura asigna `m_WiresJSON` desde `GetWiresJSON`, `m_SavedJSON` desde el getter de
     ultimo, `m_SavedPrevJSON` desde el de anterior y `m_LoadedJSON` desde el de cargado; KEEP no quita el
     id de `m_Dirty` y llama a `LFPG_InvalidateWireJSONCache(`; el replay toma L del getter de cargado y
     lo compara con los cuatro campos; `ReplayAtBoot` llama a `TryReadRawJsonVersion(` y
     `LFPG_WireJournalFileVerdict(` para los cuatro candidatos antes del primer `LoadFile(` o
     `DeleteFile(`, carga en el orden `.tmp`, principal, `.bak.new`, `.bak`, y no tiene ningun `CopyFile(`;
     el primer argumento de cada `DeserializeJSON(` es un local `new array<ref LFPG_WireData>`; `Tick` hace
     return por inactivo antes de cualquier `AtomicSaveWireJournal(`; `ReplayAtBoot` hace return si
     `m_Loaded` antes de leer nada; `MarkOwnerDirty` no menciona `IsValidationActive` y `Tick` lo mira
     antes de capturar. Convenciones: ni `+=`, `-=`, `++`, `--`, ternario, `foreach` ni `Print(`; ningun
     `ref` en locales ni parametros.
   - Mutantes, cada uno hace fallar el test: captura con ultimo en `m_SavedPrevJSON`; replay con el getter
     de ultimo; KEEP que quita el id de `m_Dirty`; sondeo de version solo del principal; carga del
     principal antes que `.tmp`; `Tick` sin la guarda de inactivo; deserializar sobre `GetDeviceWires(`;
     `MarkOwnerDirty` con la guarda de validacion.

## Gates antes de cada commit

Los de `BRIEF.md` (linter con delta de errores 0 y `ui_reconcile` 0 FAIL; barrido de campos no
declarados, incluidos los getters nuevos; convenciones; gate A de EOL: `LFPG_NetworkManagerImpl.c` CRLF,
el resto LF y el fichero nuevo LF; todos los `test_*.py` en verde antes y despues del commit; diff
minimo, cada linea traza a D1-D5 o a los tests).

## Entrega

Antes de implementar: R22 de Codex de esta v3 contra la rama, con el contrato y las garantias como marco
(un historial que el contrato declara fuera, o que G3 o G4 aceptan, no es hallazgo; uno que da un
resultado mas viejo que lo que cargo la CE dentro del contrato, si). Salida en
`reviews/2026-11-08-nostra-131/REVIEW-codex-plan-diario-v3.md`. Luego commit con "(Grok, ronda N)" (el
numero que toque), sin version de modelo, rutas explicitas, y push. Revision de Codex (R21) del commit
congelado, centrada en: que el arranque nunca pise un estado de la CE fuera de los cuatro campos; que
ninguna emision distinta retire una entrada; DURABLE solo en la segunda emision de W; cadenas vacias;
KEEP y la cache; lectura de los cuatro candidatos sin promover y version futura intacta; marcado durante
validaciones; almacenes congelados; el Generator; bytes de la CE; coste de los enganches. Salida en
`reviews/2026-11-08-nostra-131/REVIEW-codex-diario-r1.md`. Hallazgos con repro o `path:line`, ronda
siguiente. No arranques DayZ para esto: la prueba en partida es `PRUEBA-DIARIO.md`.

## Como cierra la v3 lo que encontro la revision de plan v2

| Hallazgo | Cierre |
|---|---|
| D2-01 estados desplazados por emisiones | C1-C3 medidos: lo que la CE puede cargar es la ultima emision o la anterior, y la entrada guarda las dos mas el estado cargado de la sesion (S9). Una generacion mas vieja queda fuera y da STALE, sin tocar (S10, G1). |
| D2-02 DURABLE por tiempo | Fuera `PASS_GAP`: DURABLE en la segunda emision de W, que por C1 y C2 esta en otra pasada con la primera ya terminada. Si C1 o C2 fallan en otro servidor, G4: como sin diario. |
| D2-03 escritor y lector incompatibles | Se leen los cuatro ficheros que deja el guardado (`.tmp`, principal, `.bak.new`, `.bak`), del mas nuevo al mas viejo, sin promover. J8c. |
| D2-04 version futura de un respaldo | Se mira la version de los cuatro antes de cargar o borrar ninguno; uno futuro deja todo intacto, tambien desactivado. J8d. |
| D2-05 KEEP pierde el reintento | KEEP deja el id marcado e invalida la cache del dueno (S6). El getter que guarda el fallo va al bug-ledger. |
| M2-01 `savedEmpty` | Texto de CaptureKind corregido; S7 y S11. |
| M2-02 linea `first-emit` | En D4.2. |
| O2-01 seleccion de `.002` | Fuera del contrato (C3): STALE sin tocar. |
| O2-02 RECAPTURE permanente | C4: el JSON es estable salvo una vez tras arrancar. Ademas ya no hay RECAPTURE: una emision distinta no hace nada y nunca escribe. |
| O2-03 el modelo puede tapar el gestor | El test de estructura fija las asignaciones reales del gestor, con sus mutantes. |
| h) L desplazado antes del replay | L es el estado cargado, que no cambia en la sesion (S13). |
| Sonda: rafagas del arranque a 24-27 s | Antes del replay el diario no esta activo y no cuenta esas emisiones; las entradas restauradas se recapturan tras la validacion (S3). |
| D3-01 deshacer dentro de la ventana | G5 declarada y G1 acotado; propiedad (a) de la prueba con semilla. |
| D3-02 ultimo vacio tras una emision fallida | `CaptureKind` con `loadedEmpty`; S14, S15 y propiedad (b). |
| M3-01 enganche sin guarda | `if (!owner) return;` en `QueueBroadcastOwnerSnapshotFromWires`. |
| O3-01 el modelo puede tapar el despacho | Las transiciones reales de `OnOwnerSaved`, con su mutante. |
