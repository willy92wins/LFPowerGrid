Voy a leer el brief y las skills que pida, y luego ejecuto PARTE A y PARTE B con el formato que exige.El brief pide verificación contra HEAD y un censo de acoplamientos V3. Empiezo por confirmar el commit y localizar los símbolos de S04, S08 y el sorter.El grep ya sitúa S04 y S08 en los `Impl`. Ahora leo esos cuerpos y completo el censo V3 fuera de la propia V3.S04 sigue con el cursor global al agotar presupuesto. Sigo con el RPC de repack, el tope 200 y el censo V3 fuera de los ficheros de producción.Verificado contra HEAD `421cabb` en `sorter/v4-finish`. Los números de línea de las fichas (commit `d61705e`) ya no aplican: la lógica está en los `Impl` y en `scripts/5_Mission/LFPG_SorterLogic.c`.

---

### S04 — VIVO
- **Dónde ahora:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:6193-6197` (agotamiento); `:6272-6276` (avance solo si no se agotó); presupuesto en `scripts/3_Game/LFPG_Defines.c:354-355`.
- **Qué veo:**

```6193:6197:scripts/5_Mission/LFPG_NetworkManagerImpl.c
            if (budgetExhausted)
            {
                m_SorterCursor = sorterIndex;
                break;
            }
```

```6272:6276:scripts/5_Mission/LFPG_NetworkManagerImpl.c
        if (!budgetExhausted)
        {
            m_SorterCursor = batchEnd;
            if (m_SorterCursor >= total)
                m_SorterCursor = 0;
        }
```

```354:355:scripts/3_Game/LFPG_Defines.c
static const int LFPG_SORTER_MAX_EVAL       = 20;     // max rule evaluations per sorter per tick
static const int LFPG_SORTER_RULECHECK_BUDGET = 512;  // max actual rule checks + config cache misses per scheduler tick
```

- **Razonamiento:** El presupuesto 512 sigue siendo global al tick (`budgetRemaining = 512 - ruleChecksTick - configMissesTick` en `:6130`). `EvaluateItemBudgeted` marca `itemDeferred` al superarlo (`LFPG_SorterLogic.c:432-437`). Entonces el cursor global se queda en el sorter actual, no en el siguiente, y el `for` de la tanda se corta. El tick siguiente vuelve a empezar por el mismo aparato (`:5964`). Hay reanudación local (`m_SorterResumeItems` / reglas / salidas en `:6135-6141`), que es justo lo que la ficha pedía conservar; no rota la cola global. Con 48 reglas que no coinciden, un solo sorter puede gastar las 512 comprobaciones antes de `MAX_EVAL` 20 y privar al resto. El modelo 1000×48 → ≥94 porciones de 512 a 5 s sigue siendo tamaño de cola, no un benchmark.
- **Discrepancia con la ficha:** ninguna sobre el defecto. Sí cambió el sitio: de `LFPG_NetworkManager.c` a `LFPG_NetworkManagerImpl.c`.

---

### S08 — VIVO
- **Dónde ahora:** tope 200 solo en la evaluación previa (`LFPG_NetworkManagerImpl.c:6497-6511`); `RepackCargoInPlace` síncrono e ilimitado (`:6476` y `:6566`; cuerpo en `LFPG_SorterLogic.c:1105-1327`); RPC con distancia y cooldown (`LFPG_RPCServerHandlerImpl.c:2649-2688`).
- **Qué veo:**

```6497:6511:scripts/5_Mission/LFPG_NetworkManagerImpl.c
        int moved = 0;
        int evaluated = 0;
        int maxEval = 200;
        // ...
        for (ci = 0; ci < sortCache.Count(); ci = ci + 1)
        {
            if (evaluated >= maxEval)
                break;
```

```6565:6566:scripts/5_Mission/LFPG_NetworkManagerImpl.c
        // v5.0: Repack remaining items in source (in-place, no ground round-trip)
        LFPG_SorterLogic.RepackCargoInPlace(container);
```

```1290:1296:scripts/5_Mission/LFPG_SorterLogic.c
                il_src = new InventoryLocation;
                cItem.GetInventory().GetCurrentInventoryLocation(il_src);

                il_dst = new InventoryLocation;
                il_dst.SetCargo(container, cItem, 0, placedRow[idx], placedCol[idx], placedFlip[idx]);

                moveOk = GameInventory.LocationSyncMoveEntity(il_src, il_dst);
```

- **Razonamiento:** `maxEval = 200` no envuelve el repack. `RepackCargoInPlace` recoge los N ítems, ordena por inserción O(N²) (`:1159-1182`), coloca en rejilla virtual con `TryPlaceOnGrid` (barrido de celdas, `:1339-1384`), y hasta N pasadas de N candidatos (`maxPasses = n`, `:1252-1307`). Cada intento hace `new InventoryLocation` dos veces, también si el movimiento falla. No hay cupo de operaciones ni cola incremental. El RPC sí recorta: `AllowPlayerAction` (ventana + cooldown por jugador, `:2654`) y `dist > LFPG_INTERACT_DIST_M` (5 m, `:2683-2688`). Eso no acota el trabajo del scheduler ni el del bin-pack. El mismo `HandleSorterRequestSort` lo usa también `SORTER_TEST_REQUEST_SORT` (`RPCServerHandlerImpl.c:147-150`), así que V4 TEST hereda el mismo pico.
- **Discrepancia con la ficha:** ninguna sobre el defecto. Los paths viejos (`4_World/LFPG_SorterLogic.c`, `LFPG_NetworkManager.c`, `LFPG_RPCServerHandler.c`) ahora son `5_Mission/LFPG_SorterLogic.c` y los dos `Impl`.

---

## PARTE B — Censo V3 y bloqueadores

Los tres bloqueadores conocidos **siguen en HEAD**:

**(a) Confirmado.** `SORTER_TEST_RESYNC = 65` tiene receptor (`LFPG_RPCServerHandlerImpl.c:157-160`), guard (`LFPG_RPCGuard.c:66`) y ACK cliente (`LFPG_RPCClientHandler.c:79-81`). Ningún emisor: el único `Write` de resync es V3 `LFPG_ActionSyncSorter.c:101` → `SORTER_RESYNC` (29). `SORTER_TEST_CARGO_REFRESH = 70` tiene receptor (`RPCClientHandler.c:91-93`, `:976`) y **ningún productor**: `BroadcastCargoRefreshToNearby` fija `refreshSubId = LFPG_RPC_SubId.SORTER_CARGO_REFRESH` (34) en `LFPG_NetworkManagerImpl.c:6316`. Esto no impide borrar los `.c` V3; sí impide que V4 sustituya del todo el cableado RPC al jubilar V3.

**(b) Confirmado, y más amplio.** No solo `LFPG_BTCAtmController.c`: también `LFPG_BTCAtmView.c` (64 usos de `COL_*` + la clase `LFPG_ColorData` definida en `LFPG_SorterView.c`).

**(c) Confirmado.** `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69` miran solo `LFPG_SorterView.IsOpen()`. `LFPG_ActionOpenSorterPanel.c:73-75` y `LFPG_ActionOpenSorterPanel_TEST.c:65-67` ya miran las dos.

`config.cpp` **no nombra** `LFPG_SorterView` / `Controller` / `TagView` / `PreviewRow` / `ColorData`. Los `.layout` V3 son artefactos propios de esa UI.

### Tabla de acoplamientos (símbolos V3 desde ficheros que no son los cuatro de producción)

| símbolo V3 | fichero:línea que lo usa | qué necesita de la V3 | ¿bloquea el borrado? | sustituto V4 si existe |
|---|---|---|---|---|
| `LFPG_SorterView.Init` | `scripts/5_Mission/LFPG_MissionInit.c:148` | precrea el panel V3 | sí | `LFPG_SorterView_TEST.Init` (hoy lazy en `Open`, `:1310`) |
| `LFPG_SorterView.IsOpen` / `HandleEscKey` | `LFPG_MissionInit.c:176,180` | traga teclas con el panel V3 abierto | sí | `_TEST.IsOpen` / `HandleEscKey` ya en `:185-189` |
| `LFPG_SorterView.IsOpen` / `IsEscCooldown` | `LFPG_MissionInit.c:227,229` | no abrir menú pausa al soltar ESC | sí | `_TEST` ya en `:231-233` |
| `LFPG_SorterView.IsOpen` / `Close` | `LFPG_MissionInit.c:250,268` | cierra V3 si muere/inconsciente | sí | bloque `_TEST` en `:272+` |
| `LFPG_SorterView.IsOpen` | `LFPG_MissionInit.c:275` | dual-open: TEST cede si V3 está abierta | sí (hay que quitar el yield) | no hace falta tras borrar V3 |
| `LFPG_SorterView.Cleanup` | `LFPG_MissionInit.c:473` | teardown de misión | sí | `_TEST.Cleanup` ya en `:475` |
| `LFPG_SorterView.Open` | `LFPG_RPCClientHandler.c:568` | abre UI V3 al `CONFIG_RESPONSE` | sí | `_TEST.Open` en `:937` |
| `LFPG_SorterView.OnSaveAck` | `LFPG_RPCClientHandler.c:580` | ACK de guardado V3 | sí | `_TEST.OnSaveAck` `:949` |
| `LFPG_SorterView.OnSortAck` | `LFPG_RPCClientHandler.c:592` | ACK de sort V3 | sí | `_TEST.OnSortAck` `:961` |
| `LFPG_SorterView.OnPreviewData` | `LFPG_RPCClientHandler.c:729` | preview V3 | sí | `_TEST.OnPreviewData` `:1098` |
| `LFPG_SorterView.IsOpen` | `LFPG_Actions.c:528` | bloquea cableado con panel V3 abierto; **no** consulta TEST | sí | `LFPG_SorterView_TEST.IsOpen` (hay que añadirlo; hoy no está) |
| `LFPG_SorterView.IsOpen` | `LFPG_ActionSyncSorter.c:69` | no resync V3 con UI abierta; **no** consulta TEST | sí | idem; la acción emite solo `SORTER_RESYNC` (29), no 65 |
| `LFPG_SorterView.IsOpen` | `LFPG_ActionOpenSorterPanel.c:73` | no abrir V3 si ya hay panel | sí mientras viva la entidad `LFPG_Sorter` | ya combina con `_TEST.IsOpen` `:75` |
| `LFPG_SorterView.IsOpen` | `LFPG_ActionOpenSorterPanel_TEST.c:67` | dual-open desde la acción TEST | sí hasta borrar el guard | `_TEST.IsOpen` `:65` |
| `LFPG_SorterView.IsOpen` | `LFPG_SorterView_TEST.c:1329` | `Open` TEST aborta si V3 está abierta | sí hasta borrar el guard | ninguno (es el mutex anti dual-open) |
| `LFPG_SorterView.COL_RED` | `LFPG_BTCAtmController.c:186,803` | paleta ATM | sí | `LFPG_SorterView_TEST.COL_RED` (`:257`); mejor extraer paleta compartida, no acoplar el ATM a TEST |
| `LFPG_SorterView.COL_GREEN` | `LFPG_BTCAtmController.c:190,807` | paleta ATM | sí | `_TEST.COL_GREEN` `:252` |
| `LFPG_SorterView.COL_RED_BTN` | `LFPG_BTCAtmController.c:418` | paleta ATM | sí | `_TEST.COL_RED_BTN` `:266` |
| `LFPG_SorterView.COL_BTN` | `LFPG_BTCAtmController.c:421,424` | paleta ATM | sí | `_TEST.COL_BTN` `:258` |
| `LFPG_SorterView.COL_TEXT_DIM` | `LFPG_BTCAtmController.c:436,437` | paleta ATM | sí | `_TEST.COL_TEXT_DIM` `:260` |
| `LFPG_SorterView.COL_TEXT` | `LFPG_BTCAtmController.c:445` | paleta ATM | sí | `_TEST.COL_TEXT` `:259` |
| `LFPG_SorterView.COL_TEXT_MID` | `LFPG_BTCAtmController.c:446` | paleta ATM | sí | `_TEST.COL_TEXT_MID` `:261` |
| `LFPG_SorterView.COL_BG_PANEL` | `LFPG_BTCAtmView.c:567` | paleta ATM | sí | `_TEST.COL_BG_PANEL` `:247` |
| `LFPG_SorterView.COL_HEADER` | `LFPG_BTCAtmView.c:568,640` | paleta ATM | sí | `_TEST.COL_HEADER` `:262` |
| `LFPG_SorterView.COL_GREEN` | `LFPG_BTCAtmView.c:569,587,588,599,636` | paleta ATM | sí | `_TEST.COL_GREEN` |
| `LFPG_SorterView.COL_BTN` | `LFPG_BTCAtmView.c:570,620,628,659,666` | paleta ATM | sí | `_TEST.COL_BTN` |
| `LFPG_SorterView.COL_TEXT_DIM` | `LFPG_BTCAtmView.c:571,574,581,582,583,584,593,598,641,642,660,667` | paleta ATM | sí | `_TEST.COL_TEXT_DIM` |
| `LFPG_SorterView.COL_AMBER` | `LFPG_BTCAtmView.c:572,585,589,594` | paleta ATM | sí | `_TEST.COL_AMBER` `:256` |
| `LFPG_SorterView.COL_TEXT` | `LFPG_BTCAtmView.c:573,597,602,613,614,621,622,629,630,658,665` | paleta ATM | sí | `_TEST.COL_TEXT` |
| `LFPG_SorterView.COL_BG_ELEVATED` | `LFPG_BTCAtmView.c:577-580,592` | paleta ATM | sí | `_TEST.COL_BG_ELEVATED` `:249` |
| `LFPG_SorterView.COL_BLUE` | `LFPG_BTCAtmView.c:586` | paleta ATM | sí | `_TEST.COL_BLUE` `:255` |
| `LFPG_SorterView.COL_INPUT_BORDER` | `LFPG_BTCAtmView.c:595,600` | paleta ATM | sí | `_TEST.COL_INPUT_BORDER` `:250` |
| `LFPG_SorterView.COL_BG_INPUT` | `LFPG_BTCAtmView.c:596,601` | paleta ATM | sí | `_TEST.COL_BG_INPUT` `:250` |
| `LFPG_SorterView.COL_SEPARATOR` | `LFPG_BTCAtmView.c:608,639` | paleta ATM | sí | `_TEST.COL_SEPARATOR` `:261` |
| `LFPG_SorterView.COL_GREEN_BTN` | `LFPG_BTCAtmView.c:611,664` | paleta ATM | sí | `_TEST.COL_GREEN_BTN` `:264` |
| `LFPG_SorterView.COL_RED_BTN` | `LFPG_BTCAtmView.c:612` | paleta ATM | sí | `_TEST.COL_RED_BTN` |
| `LFPG_SorterView.COL_TEXT_MID` | `LFPG_BTCAtmView.c:615,616,623,624,631,632` | paleta ATM | sí | `_TEST.COL_TEXT_MID` |
| `LFPG_SorterView.COL_BLUE_BTN` | `LFPG_BTCAtmView.c:619,627` | paleta ATM | sí | `_TEST.COL_BLUE_BTN` `:263` |
| `LFPG_ColorData` (clase en `LFPG_SorterView.c:45`) | `LFPG_BTCAtmView.c:36,169,702,708,719` | cache de color por `SetUserData` | **sí: borrar `LFPG_SorterView.c` tumba el ATM** | `LFPG_ColorData_TEST` en `LFPG_SorterView_TEST.c:71` (no usarlo desde ATM; extraer la clase) |
| `LFPG_SorterController` | ningún fichero fuera de V3 | — | no (si se borran los 4 `.c` juntos) | `LFPG_SorterController_TEST` |
| `LFPG_SorterTagView` | ningún fichero de código fuera de V3; comentario en `gui/layouts/LFPG_SorterTag.layout:3` | prefab del tag | no (retirar el layout con la UI) | `LFPG_SorterTagView_TEST` + `gui/layouts/test/LFPG_SorterTag_TEST.layout` |
| `LFPG_SorterPreviewRow` | ningún fichero de código fuera de V3; comentario en `gui/layouts/LFPG_SorterPreviewRow.layout:3` | prefab de preview | no (retirar el layout con la UI) | `LFPG_SorterPreviewRow_TEST` + `gui/layouts/test/LFPG_SorterPreviewRow_TEST.layout` |
| `LFPG_SorterView` (comentario de layout) | `gui/layouts/LFPG_Sorter.layout:9` | layout raíz V3 (`GetLayoutFile` → ese path en `LFPG_SorterView.c:219`) | no (es el artefacto V3) | layout TEST bajo `gui/layouts/test/` |

**Coste real de jubilar V3 (lectura de HEAD):** no es borrar 4 ficheros. `LFPG_SorterController` / `TagView` / `PreviewRow` no tienen clientes externos. Lo que obliga trabajo es: (1) extraer paleta + `LFPG_ColorData` del ATM (~75 usos de `COL_*` entre view y controller); (2) retocar `MissionInit` y los handlers V3 de `RPCClientHandler`; (3) completar `IsOpen()` TEST en cableado y resync; (4) decidir qué hace `LFPG_ActionOpenSorterPanel` / entidad `LFPG_Sorter` si la UI V3 desaparece; (5) aparte, los agujeros RPC V4 del bloqueador (a), que no son acoplamientos V3 pero sí impiden que TEST sea un reemplazo completo.

## LO QUE NO PUDE VERIFICAR

El hook de shell de este entorno bloqueó `git`; no pude correr `git diff --ignore-all-space d61705e..HEAD`. Los veredictos salen del árbol actual en `421cabb`, localizado por símbolo. No hay medición in-game del coste del repack (el brief lo prohibía). Nada más.