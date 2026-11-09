# BRIEF-DIARIO — diario de cables para apagados sucios (1.3.1, ronda 2)

Encargo de implementacion para la lane (Grok). Va **despues** de la ronda 1 (`BRIEF.md`, S1-S4), en
la misma rama `claude/project-thread-0iolln` y encima de su commit. Lee `AGENTS.md` entero; si algo
de aqui lo contradice, manda este brief y dilo en el informe.

## Contexto

Medido el 2026-10-26 (`C:\tmp\lfpg-cables-reinicio\INFORME-2.md`): si el servidor muere sin apagado
limpio (crash, `taskkill /F`, reinicio duro del host) antes del siguiente ciclo de guardado de la CE,
se pierden exactamente los cables hechos, cortados o rerouteados desde ese ciclo. No es un bug del
mod: los cables de un aparato LFPG viajan en su registro CE (`OnStoreSave`) y DayZ no tiene funcion
de script para forzar un guardado. Guillermo decidio el 2026-11-08 meter en la 1.3.1 un diario: un
fichero en el perfil del servidor con el ultimo estado de cables de cada aparato que cambio y que la
CE aun no ha guardado, que se reaplica al arrancar.

No cubre, y no hay que intentarlo: aparatos colocados despues del ultimo guardado (DayZ los pierde
enteros; los cables hacia ellos se podan al arrancar como hoy), estados de interruptores, cables de
aparatos vanilla (ya van en `vanilla_wires.json` cada 5 s).

## Diseno (leelo antes del codigo)

1. Por cada dueno de cables nativo (`LFPG_WireOwnerBase` y `LFPG_Generator`, los dos unicos con
   almacen propio) el diario guarda dos cadenas con el mismo JSON que ya escribe `OnStoreSave`:
   `m_BaseJSON`, lo ultimo que la CE guardo o cargo de ese aparato en el momento de la captura, y
   `m_WiresJSON`, su estado vivo.
2. Al arrancar, para cada entrada se compara lo que la CE ha cargado con esas dos cadenas
   (compare-and-swap):
   - igual a `m_WiresJSON`: la CE ya lo tenia; no se toca y la entrada se borra;
   - igual a `m_BaseJSON`: la CE se quedo en el estado anterior; se reaplica `m_WiresJSON`;
   - distinto de las dos: no se sabe que es mas nuevo (downgrade, backup restaurado, guardado a
     medias): **no se toca**, la entrada se borra y se avisa en el log.
   El diario nunca pisa un estado de la CE que no esperaba. Las reglas de limpieza durante la
   partida (D4) solo acotan el tamano del fichero; la seguridad la da esta comparacion.
3. Lo que se escribe en la CE no cambia ni un byte: mismas llamadas `ctx.Write`, mismo orden. El
   diario solo observa.
4. Coste solo cuando cambia un cable: marcar un id en un mapa al publicar y escribir el fichero como
   mucho una vez cada 5 s si hay algo pendiente. En cada `OnStoreSave`, una busqueda en un mapa que
   casi siempre esta vacio. Nada recorre todos los aparatos.

## D1 — Ajuste (`scripts/3_Game/LFPG_Settings.c`)

`bool WireJournal = true;` en `LFPG_ServerSettings`, con comentario como los demas, y
` WireJournal=<valor>` en la linea `Settings loaded:`, junto al `NotifyWireReplaced=` de la ronda 1.
Sin cambios en `ver`, `ValidateAndClamp` ni `Save()`.

## D2 — Datos, constantes y fichero (3_Game)

- `LFPG_Data.c`, junto a `LFPG_VanillaWireStore`:
  - `class LFPG_WireJournalEntry` con exactamente `string m_OwnerDeviceId; string m_BaseJSON;
    string m_WiresJSON;` (constructor a `""`).
  - `class LFPG_WireJournalStore` con `int ver; ref array<ref LFPG_WireJournalEntry> entries;`
    (constructor: `ver = LFPG_WIRE_JOURNAL_VER`, `entries` nuevo).
  - Nada de estado de sesion en estas clases (ni tiempos ni flags): va en mapas del gestor.
- `LFPG_Defines.c`: `LFPG_WIRE_JOURNAL_VER = 1`, `LFPG_WIRE_JOURNAL_ACK_DELAY_MS = 300000`,
  `LFPG_WIRE_JOURNAL_MAX_ENTRIES = 512`, `LFPG_WIRE_JOURNAL_DIR = "$profile:LF_PowerGrid"`,
  `LFPG_WIRE_JOURNAL_FILE = "$profile:LF_PowerGrid\\wire_journal.json"`, y los veredictos
  `LFPG_WJ_SAVE_IGNORE = 0`, `LFPG_WJ_SAVE_ACK = 1`, `LFPG_WJ_SAVE_DROP = 2`,
  `LFPG_WJ_REPLAY_MISSING = 0`, `LFPG_WJ_REPLAY_UNUSABLE = 1`, `LFPG_WJ_REPLAY_UNCHANGED = 2`,
  `LFPG_WJ_REPLAY_RESTORE = 3`, `LFPG_WJ_REPLAY_STALE = 4`.
- `LFPG_FileUtil.c`: `static bool AtomicSaveWireJournal(string targetPath, LFPG_WireJournalStore store)`
  y `static bool EnsureWireJournalFileOrRestore(string targetPath)`, copias tipadas de
  `AtomicSaveVanillaWires` / `EnsureVanillaWiresFileOrRestore` (mismo patron `.tmp`/`.bak.new`/`.bak`
  con read-back), `RECOVERY_WIRE_JOURNAL = 4` y su rama en `IsRecoveryFileReadable`. Nada mas en
  ese fichero.

Formato del fichero (lo que tiene que salir de `JsonFileLoader`):
`{"ver":1,"entries":[{"m_OwnerDeviceId":"<id>","m_BaseJSON":"<json escapado>","m_WiresJSON":"<json escapado>"}]}`

## D3 — Lo ultimo guardado de cada dueno (4_World)

- `LFPG_WireOwnerBase.c`: miembro `protected string m_LastStoredWireJSON;` (a `""` en el
  constructor). En `LFPG_OnStoreLoadExtra`: en la rama de lectura truncada, `m_LastStoredWireJSON = "";`;
  tras una lectura buena, `m_LastStoredWireJSON = json;` (la cadena tal cual se leyo). En
  `LFPG_OnStoreSaveExtra`, despues de calcular `json` en cualquiera de las dos ramas y antes de
  `ctx.Write(json)`: `m_LastStoredWireJSON = json;` y, solo si no esta congelado y dentro de
  `#ifdef SERVER`, `LFPG_WireJournal.Get().OnOwnerSaved(m_DeviceId, json);`. Getters publicos
  `string LFPG_GetLastStoredWiresJSON()` y `bool LFPG_IsWireStoreFrozen()`.
- `LFPG_TestDevices.c`, clase `LFPG_Generator` (es un item real, `config.cpp:249`): el mismo miembro
  y getter. En `OnStoreLoad`, tras leer `json` con exito, `m_LastStoredWireJSON = json;`. En
  `OnStoreSave`, tras `SerializeJSON`, `m_LastStoredWireJSON = json;` y la misma llamada a
  `OnOwnerSaved` bajo `#ifdef SERVER`.
- Los bytes que se escriben en `ctx` tienen que ser los mismos que antes.

## D4 — Gestor nuevo: `scripts/4_World/LFPG_WireJournal.c` (LF, como el resto de 4_World)

Singleton `static ref LFPG_WireJournal s_Instance;` + `static LFPG_WireJournal Get()`. La clase **no**
va dentro de `#ifdef SERVER` (la llaman 4_World y 5_Mission); los cuerpos que tocan ficheros o el
mundo, si. Estado solo de sesion: `ref map<string, ref LFPG_WireJournalEntry> m_Entries`,
`ref map<string, bool> m_Dirty`, `ref map<string, int> m_AckMs`, `ref array<string> m_Restored`,
`bool m_Loaded`, `bool m_Enabled`, `bool m_ReadOnly`, `bool m_FileDirty`, `bool m_CapWarned`,
`int m_SaveFailures`, `int m_LastSaveWarnMs`.

"Usable" para un objeto = `LFPG_DeviceAPI.HasWireStore(obj)`, su id no es
`LFPG_DeviceRegistry.Get().IsAmbiguous(id)` y, si es `LFPG_WireOwnerBase`, no esta congelado.
"Lo guardado" de un objeto = `LFPG_GetLastStoredWiresJSON()` por cast a `LFPG_WireOwnerBase` o a
`LFPG_Generator`.

Funciones puras (static, firma en una sola linea, solo escalares: son lo que cargan los tests):

```
static bool LFPG_WireJournalCaptureAllowed(bool enabled, bool readOnly, bool startupDone, bool validationActive, bool hasEntry, int entryCount, int maxEntries)
    // false si !enabled, readOnly, !startupDone o validationActive; true si hasEntry;
    // false si entryCount >= maxEntries; true en otro caso.
static int LFPG_WireJournalSaveVerdict(bool hasEntry, bool isDirty, bool savedMatchesJournal)
    // IGNORE si !hasEntry o isDirty; ACK si savedMatchesJournal; DROP en otro caso.
static bool LFPG_WireJournalAckExpired(int ackMs, int nowMs, int delayMs)
    // false si ackMs < 0; true si nowMs - ackMs >= delayMs; false en otro caso.
static int LFPG_WireJournalReplayVerdict(bool ownerFound, bool usable, bool storedEqualsJournal, bool storedEqualsBase)
    // MISSING si !ownerFound; UNUSABLE si !usable; UNCHANGED si storedEqualsJournal;
    // RESTORE si storedEqualsBase; STALE en otro caso. El orden importa: si base == wires, UNCHANGED.
```

Metodos:

1. `void MarkOwnerDirty(EntityAI owner)`: id con `LFPG_DeviceAPI.GetDeviceId`; si es `""` o no hay
   almacen, nada. Con `nm = LFPG_NetworkManager.Get()` (si es null, nada), si
   `LFPG_WireJournalCaptureAllowed(m_Enabled && m_Loaded, m_ReadOnly, nm.IsStartupValidationDone(),
   nm.IsValidationActive(), m_Entries.Contains(id), m_Entries.Count(), LFPG_WIRE_JOURNAL_MAX_ENTRIES)`
   da true, `m_Dirty[id] = true`. Si solo falla por el tope, un
   `Warn` por sesion: `[WireJournal] Entry cap <N> reached; new owners are not journaled until
   entries are acknowledged`.
2. `void OnOwnerSaved(string ownerId, string savedJSON)`: si no esta cargado, activo y escribible,
   nada. Veredicto con `LFPG_WireJournalSaveVerdict(hayEntrada, m_Dirty.Contains(id),
   hayEntrada && savedJSON == entrada.m_WiresJSON)`. ACK: si `m_AckMs[id] < 0`, guarda la hora
   (`g_Game.GetTime()`). DROP: borra la entrada y su ack, `m_FileDirty = true`. Lineas `Debug`
   `[WireJournal] ack <id>` / `[WireJournal] drop <id> superseded`.
3. `void Tick(int nowMs)` (cada 5 s, D5): **lo primero**, si no esta cargado, activo y escribible,
   return (antes de cargar no puede escribir: pisaria el fichero de la sesion anterior).
   a) Por cada id sucio: objeto con `FindById`; si no esta o no es usable, borra su entrada si la
      habia. Si no, `m_WiresJSON = LFPG_DeviceAPI.GetWiresJSON(obj)` y `m_BaseJSON` = lo guardado;
      entrada nueva solo si hay sitio bajo el tope. Si wires == base, `m_AckMs[id] = nowMs` (ya esta
      guardado; empieza el plazo); si no, `-1`. `m_FileDirty = true`. Vacia `m_Dirty`.
   b) Limpieza: borra las entradas con `LFPG_WireJournalAckExpired(m_AckMs[id], nowMs,
      LFPG_WIRE_JOURNAL_ACK_DELAY_MS)` (junta los ids y borra despues, no durante el recorrido).
   c) Si `m_FileDirty`: `MakeDirectory` si falta y `AtomicSaveWireJournal`. Si va bien,
      `m_FileDirty = false`; si falla, se queda sucio y avisa con el mismo limite de 60 s que
      `FlushVanillaIfDirty`. Una linea `Debug` por guardado: `[WireJournal] saved entries=<n> ms=<t>`.
4. `void ReplayAtBoot()`: si ya esta cargado, return (idempotente); `m_Loaded = true`.
   `m_Enabled` = ajuste `WireJournal` (true si no hay ajustes). Si esta desactivado: borra
   `wire_journal.json` y sus `.tmp`, `.bak`, `.bak.new` si existen, `Info` si habia alguno
   (`[WireJournal] Disabled by settings; journal files removed`) y return; asi, al reactivarlo no
   se reaplica nada viejo.
   Carga con `EnsureWireJournalFileOrRestore` + `JsonFileLoader.LoadFile`. Sin fichero: diario
   vacio. No legible: `Warn` y diario vacio. `ver > LFPG_WIRE_JOURNAL_VER`: `m_ReadOnly = true`,
   `Warn` (`journal schema v<x> > v<y>; ignored and left untouched`) y return.
   Por cada entrada con id no vacio: veredicto con
   `LFPG_WireJournalReplayVerdict(encontrado, usable, guardado == m_WiresJSON, guardado == m_BaseJSON)`.
   - RESTORE: `DeserializeJSON` sobre un **array local nuevo** (nunca sobre el del aparato: si la
     cadena no vale, `DeserializeJSON` vacia lo que recibe). Si devuelve false, cuentalo como
     UNUSABLE. Si no, `LFPG_DeviceAPI.ClearDeviceWires(obj)` y `AddDeviceWire` de cada cable,
     contando los rechazados. La entrada se queda (ack -1) y el id va a `m_Restored`.
     `Info`: `[WireJournal] Restored <id> wires=<n> rejected=<k>`.
   - STALE: `Warn` `[WireJournal] Skipped <id>: stored wires match neither journal state`; se borra.
   - MISSING, UNUSABLE, UNCHANGED: se borra (`Debug`).
   Resumen siempre a `Info`: `[WireJournal] Replay entries=<n> restored=<r> unchanged=<u>
   stale=<s> missing=<m> unusable=<x>`. Si se borro algo, `m_FileDirty = true`.
   No toques indice inverso, cuentas por jugador ni grafo: las fases 9 y 13 de la validacion los
   rehacen justo despues.
5. `void OnStartupValidationDone()`: cada id de `m_Restored` a `m_Dirty` y vacia `m_Restored`. Asi el
   siguiente Tick recaptura base y estado despues de la poda y del refresco de net ids.

## D5 — Enganches en `scripts/5_Mission/LFPG_NetworkManagerImpl.c` (fichero CRLF: cada linea nueva en CRLF)

1. `LFPG_WireJournal.Get().MarkOwnerDirty(owner);` como primera sentencia tras la guarda de `owner`
   nulo (o la primera, si no hay guarda) de `BroadcastOwnerWires`, `BroadcastOwnerWireDelta`,
   `QueueBroadcastOwnerSnapshot` y `QueueBroadcastOwnerSnapshotFromWires`. Todo cambio de cables
   de un dueno nativo en partida acaba en una de las cuatro (FinishWiring, reroute, CutWires,
   CutPort, RemoveWiresTargeting, CutAll, podas). El broadcast de init de un aparato nuevo tambien
   pasa por ahi y solo crea una entrada inofensiva.
2. `ValidateAllWiresAndPropagate()`, rama de servidor, justo despues del bloque
   `if (m_ValidationActive) {...}`: `if (!m_StartupValidationDone) LFPG_WireJournal.Get().ReplayAtBoot();`
3. `LFPG_ValidationFinalize()`: dentro de `if (wasStartupPending)` (ponle llaves),
   `LFPG_WireJournal.Get().OnStartupValidationDone();`.
4. `LFPG_ServerSchedulerTick()`: en el bloque `m_SchedFlushMs >= LFPG_VANILLA_FLUSH_S * 1000`, despues
   de `FlushVanillaIfDirty();`, `LFPG_WireJournal.Get().Tick(g_Game.GetTime());`.

Nada mas en ese fichero.

## Fuera de alcance

Formato CE, `vanilla_wires.json`, grafo, handlers de RPC (`LFPG_RPCServerHandlerImpl.c` no se toca
en esta ronda), `config.cpp` (comprueba solo que `worldScriptModule` carga la carpeta 4_World, asi
que el fichero nuevo entra solo). Nada adyacente se arregla: va al informe. Uno ya visto para el
bug-ledger: `LFPG_Generator.OnStoreLoad` ignora el false de `DeserializeJSON` (cables de esquema
futuro o ilegibles), y su siguiente guardado los borra. Anotalo, no lo arregles.

## Tests offline (R26) — antes o junto al codigo, y tienen que morder

En `.github/tools/`, estilo `test_finish_wiring_quota.py` (`enforce_scalar_slice.load/method`,
constantes como stubs leidas de `LFPG_Defines.c`, sin historia de git: lo viejo como literal o
mutante del fuente actual). Registralos en `.github/workflows/checks.yml`. Salen con 0 en verde y
distinto de 0 en rojo (`unittest`).

1. `test_wire_journal_rules.py`, las cuatro funciones puras del fuente:
   - CaptureAllowed: P1 `(true,false,true,false,false,0,512)` true. P2 con entrada en el tope
     `(…,true,512,512)` true. P3 limite `(…,false,511,512)` true. N1 enabled false. N2 readOnly.
     N3 startupDone false. N4 validationActive. N5 tope sin entrada `(…,false,512,512)` false.
     Mutante sin el true por `hasEntry`: P2 falla.
   - SaveVerdict: `(false,*,*)` IGNORE; `(true,true,false)` e `(true,true,true)` IGNORE;
     `(true,false,true)` ACK; `(true,false,false)` DROP. Mutante sin la comprobacion de sucio:
     `(true,true,false)` da DROP y el test lo caza.
   - AckExpired: `(-1, 10^9, D)` false; `(0, D-1, D)` false; `(0, D, D)` true;
     `(1000, 1001+D, D)` true. Mutante `>` en vez de `>=`: el limite falla.
   - ReplayVerdict: `(false,…)` MISSING; `(true,false,…)` UNUSABLE; `(true,true,true,false)` y
     `(true,true,true,true)` UNCHANGED; `(true,true,false,true)` RESTORE; `(true,true,false,false)`
     STALE. Mutante que mira la base antes que el estado: `(true,true,true,true)` da RESTORE.
2. `test_wire_journal_structure.py`, sobre los fuentes:
   - `bool WireJournal = true;` y `WireJournal=` en la linea `Settings loaded:`.
   - Las dos clases de datos con exactamente esos campos; los dos metodos tipados de `FileUtil` y la
     rama nueva de `IsRecoveryFileReadable`.
   - En las cuatro funciones de D5.1, `MarkOwnerDirty(owner)` aparece antes de la primera de
     `CoalescePendingOwnerSnapshot(`, `StorePendingOwnerSnapshot(`, `CaptureOwnerBroadcastSnapshot(`
     o `.Send(` de su cuerpo. La unica llamada a `ReplayAtBoot(` fuera de su definicion esta en
     `ValidateAllWiresAndPropagate`, tras `!m_StartupValidationDone`. `OnStartupValidationDone(`
     dentro de `if (wasStartupPending)`. `Tick(` en el mismo bloque que `FlushVanillaIfDirty();`.
   - `m_LastStoredWireJSON` se asigna en las dos ramas de carga de `LFPG_WireOwnerBase`, en su
     guardado y en carga y guardado del Generator; `OnOwnerSaved(` en los dos guardados. La lista de
     `ctx.Write(` de `LFPG_OnStoreSaveExtra` (solo `json`) y de `LFPG_Generator.OnStoreSave`
     (`m_DeviceIdLow`, `m_DeviceIdHigh`, `m_SourceOn`, `json`) es esa, en ese orden.
   - En `LFPG_WireJournal.c`: el primer argumento de cada `DeserializeJSON(` es un local creado con
     `new array<ref LFPG_WireData>`; `Tick` hace return por `!m_Loaded` antes de cualquier
     `AtomicSaveWireJournal(`; `ReplayAtBoot` hace return si `m_Loaded` antes de cargar nada.
     Convenciones: ni `+=`, `-=`, `++`, `--`, ternario, `foreach` ni `Print(`; ningun `ref` en
     locales ni parametros.
   - Mutantes: el fuente del gestor sin la guarda de `!m_Loaded` en `Tick`, y uno que deserializa
     sobre `GetDeviceWires(`, hacen fallar el test.

## Gates antes de cada commit

Los de `BRIEF.md` (linter con delta de errores 0 y `ui_reconcile` 0 FAIL; barrido de campos no
declarados, incluidos los nuevos getters y `IsValidationActive` en la base 4_World; convenciones;
gate A de EOL: `LFPG_NetworkManagerImpl.c` CRLF, el resto LF y el fichero nuevo LF; todos los
`test_*.py` en verde antes y despues del commit; diff minimo, cada linea traza a D1-D5 o a los tests).

## Entrega

Commit en `claude/project-thread-0iolln` con "(Grok, ronda 2)", sin version de modelo, rutas
explicitas, y push. Revision de ChatGPT (Codex, R21) del commit congelado, centrada en: que el
compare-and-swap no pueda pisar nunca un estado de la CE inesperado; orden de arranque (ningun Tick
escribe antes de `ReplayAtBoot`; el replay va antes de las fases); almacenes congelados; el camino
del Generator; que los bytes de la CE no cambian; guardado atomico y recuperacion; tope de tamano;
coste de los enganches. Guarda su salida en `reviews/2026-11-08-nostra-131/REVIEW-codex-diario-r1.md`.
Los hallazgos con repro o `path:line` vuelven en una ronda 3. No arranques DayZ para esto: la prueba
en partida es `PRUEBA-DIARIO.md`.
