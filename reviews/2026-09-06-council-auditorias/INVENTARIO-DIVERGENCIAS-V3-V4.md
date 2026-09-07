# INVENTARIO DE DIVERGENCIAS V3 ↔ V4 — Sorter

Encargo: `BRIEF-P0-DIVERGENCIAS.md`. Solo lectura: no se editó código, no se ejecutó git.
Pares comparados (5 ficheros `.c` + layouts de referencia):

| V3 (producción) | V4 (nueva) |
|---|---|
| `scripts/4_World/LFPG_SorterView.c` | `scripts/4_World/test/LFPG_SorterView_TEST.c` |
| `scripts/4_World/LFPG_SorterController.c` | `scripts/4_World/test/LFPG_SorterController_TEST.c` |
| `scripts/4_World/LFPG_SorterTagView.c` | `scripts/4_World/test/LFPG_SorterTagView_TEST.c` |
| `scripts/4_World/LFPG_SorterPreviewRow.c` | `scripts/4_World/test/LFPG_SorterPreviewRow_TEST.c` |
| `scripts/4_World/LFPG_ActionOpenSorterPanel.c` | `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c` |

`LFPG_Sorter_TEST` hereda de `LFPG_Sorter` (`scripts/4_World/test/LFPG_Sorter_TEST.c:31`), por lo que
`LFPG_IsPowered()` y `LFPG_IsLinked()` son los mismos en ambas ramas. Sub-ids RPC distintos:
V3 = 19/21/22/31, V4 = 60/62/63/67 (`scripts/3_Game/LFPG_Defines.c:294-333`).

## Tabla resumen

| Métrica | Valor |
|---|---|
| Nº de divergencias de comportamiento | 14 |
| Favorecen a V3 | 5 |
| Favorecen a V4 | 5 |
| No decidibles / equivalentes | 4 |

**Hallazgo más valioso:** la V3 se comporta mejor en **D-01** (la acción exige `powered` + `linked`)
y **D-02** (escalado DPI/resolución vía `LFPG_UIScaler`). Borrar la V3 sin propagar esas dos guardas
a la V4 es una **regresión silenciosa**: el panel V4 aparecería sobre sorters sin energía y sin
contenedor enlazado, y se vería mal en resoluciones distintas a 1080p.

---

## D-01 — La acción exige `powered` + `linked` en V3; V4 no exige ninguno
- **V3:** `scripts/4_World/LFPG_ActionOpenSorterPanel.c:55-83` —
  `if (targetObj.GetType() != "LFPG_Sorter") return false;` + `if (!sorter.LFPG_IsPowered()) return false;`
  + `if (sorter.IsRuined()) return false;` + `if (!sorter.LFPG_IsLinked()) return false;`
- **V4:** `scripts/4_World/test/LFPG_ActionOpenSorterPanel_TEST.c:50-72` —
  `if (!targetObj.IsKindOf("LFPG_Sorter_TEST")) return false;` + `if (sorter.IsRuined()) return false;`
  (sin `LFPG_IsPowered()` ni `LFPG_IsLinked()`)
- **Qué cambia si borramos la V3:** la acción V4 aparece sobre sorters sin energía y sin contenedor
  enlazado. El usuario abre el panel y ve el overlay "UNLINKED" o el estado "NO POWER" (la V4 añade
  la guarda de edición `CanEdit()` en el controller, D-03), pero la acción ya no filtra en la capa
  de interacción. Además V4 usa `IsKindOf` (más amplio) frente a `GetType()` exacto de V3.
- **Dirección:** V3
- **Riesgo:** alto

## D-02 — Escalado DPI/resolución (`LFPG_UIScaler`) existe en V3, ausente en V4
- **V3:** `scripts/4_World/LFPG_SorterView.c:1244-1248` (Init: `LFPG_UIScaler.Capture(SorterPanel)`),
  `:1407-1409` (DoOpen: `ComputeScale` + `Apply`), `:1332` (Cleanup: `LFPG_UIScaler.Reset()`).
  Tags: `scripts/4_World/LFPG_SorterTagView.c:128-134` (`ScaleWidget(tagRoot, tagScale)` con `m_Scaled`).
  Preview rows: `scripts/4_World/LFPG_SorterPreviewRow.c:118-123` (`ScaleWidget(rowRoot, rowScale)`).
- **V4:** ausente. `LFPG_SorterView_TEST` no referencia `LFPG_UIScaler` (Init/DoOpen/Cleanup sin
  Capture/Apply/Reset); `LFPG_SorterTagView_TEST` y `LFPG_SorterPreviewRow_TEST` no escalan su raíz.
- **Qué cambia si borramos la V3:** en resoluciones distintas a 1080p (o DPI > 100 %) el panel V4
  se ve desproporcionado: botones/textos pequeños o grandes, sin corrección. La V3 captura valores
  de diseño y los re-aplica en cada `Open`.
- **Dirección:** V3
- **Riesgo:** alto

## D-03 — Guarda de edición `CanEdit() = paired && powered` solo en V4
- **V3:** `scripts/4_World/LFPG_SorterController.c` — todos los handlers usan `if (!m_IsPaired) return;`
  (p. ej. `:911`, `:933`, `:981`, `:1010`, `:1042`, `:1064`, `:1085`). No existe `m_IsPowered`.
- **V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:927` —
  `protected bool CanEdit() { return m_IsPaired && m_IsPowered; }` + `m_IsPowered` consultado en
  `InitFromRPC` (`:510-520`) y re-pollado cada 0.5 s en `RefreshPowerState` (`:960-1000`).
  Todos los handlers usan `if (!CanEdit()) return;`.
- **Qué cambia si borramos la V3:** la V4 bloquea la edición de reglas si el sorter pierde energía
  mientras el panel está abierto (re-poll cada 0.5 s). La V3 permitía editar sin energía (solo
  exigía `paired`). Es una guarda más estricta: comportamiento más seguro, pero distinto.
- **Dirección:** V4
- **Riesgo:** medio

## D-04 — Hook de comandos MCP de test (widget extra + poll por frame + escritura a profile)
- **V3:** ausente.
- **V4:** `scripts/4_World/test/LFPG_SorterView_TEST.c:119-121` (campos `m_McpCmd*`),
  `:378-381` (poll en `Update`), `:1541` (`EnsureMcpCmdWidget` en `DoOpen`),
  `:1894-1969` (crea `LFPG_MCP_SorterCmd.layout` en el workspace),
  `:1976-1990` (`PollMcpSorterCmd` lee, trim, dispatch), `:2000-2066` (`DispatchMcpCommand`:
  `dump`/`close`/`catch_all`/`tab_preview`/`cat:N`), `:2103-2155` (`WriteMcpDump` escribe
  `$profile:lfpg_sorter_mcp.json`).
- **Qué cambia si borramos la V3:** si la V4 pasa a producción tal cual, se cuela infra de test
  que crea un `EditBoxWidget` ajeno al panel, lo polea cada frame, reacciona a comandos y escribe
  un JSON en `$profile` en cada comando. No rompe nada, pero es superficie de ataque y ruido
  que la V3 no tiene.
- **Dirección:** V3
- **Riesgo:** medio

## D-05 — Sonda `S1_PROBE` imprime ~16 métricas en cada `Open`
- **V3:** ausente.
- **V4:** `scripts/4_World/test/LFPG_SorterView_TEST.c:246` (`static const bool S1_PROBE = true;`),
  `:1561-1563` (`if (S1_PROBE) { RunS1Probe(); }` en `DoOpen`),
  `:1577-1648` (`RunS1Probe` imprime `sorterpanel_size_w/h`, `pos_x/y`, `screensize`, `screenpos`,
  `outputrailbg_*` vía `Print`).
- **Qué cambia si borramos la V3:** cada apertura del panel V4 escupe ~16 líneas de diagnóstico
  al RPT. Inofensivo pero es basura de test en el path de producción.
- **Dirección:** V3
- **Riesgo:** medio

## D-06 — `LoadImageFile(PROC_WHITE)` + flags de primer-paso en V3; V4 delega al layout
- **V3:** `scripts/4_World/LFPG_SorterView.c:746-749` (`Tint` hace `LoadImageFile(0, PROC_WHITE)`
  si `!m_ColorsInitialized`), `:733` (`m_ColorsInitialized = true` al final de `ApplyColors`),
  `scripts/4_World/LFPG_SorterController.c:788-791` (`StatusDot.LoadImageFile` con `m_StatusDotLoaded`).
- **V4:** `scripts/4_World/test/LFPG_SorterView_TEST.c:819-821` (`Tint` solo `SetColor`, sin
  `LoadImageFile`, sin `m_ColorsInitialized`); `LFPG_SorterController_TEST.c:774-778` (`SetStatus`
  solo `SetColor` en `StatusDot`, sin `LoadImageFile` ni `m_StatusDotLoaded`). El layout V4
  pre-mezcla colores en `ImageWidgetClass` vía propiedad `color` (verificado en
  `gui/layouts/test/LFPG_Sorter_TEST.layout:28-29`, `:42-43`).
- **Qué cambia si borramos la V3:** la V4 depende de que los `ImageWidget` del layout pinten con
  la propiedad `color` sin textura. La V3 garantiza el blanco procedural y luego lo tiñe. Si un
  cambio de motor cambia cómo renderiza un `ImageWidget` sin textura, la V4 se rompe y la V3 no.
- **Dirección:** no decidible
- **Riesgo:** medio

## D-07 — `BtnClearOut` con doble clic de confirmación solo en V4
- **V3:** `scripts/4_World/LFPG_SorterController.c:1064-1068` — `BtnClearOut` ejecuta
  `outCfg.ClearRules()` inmediatamente (sin confirmación).
- **V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:1132-1153` — primer clic arma
  `m_ClearConfirmActive = true`, `m_ClearTimer = 3.0`, label "CONFIRM?", tinte amber; segundo
  clic ejecuta. `CancelClearConfirm` (`:953-958`) restaura label/tinte.
- **Qué cambia si borramos la V3:** la V4 añade una confirmación que la V3 no tiene. Menos
  borrados accidentales, pero cambia el modelo de interacción (dos clics para limpiar).
- **Dirección:** V4
- **Riesgo:** bajo

## D-08 — `SelectOutput` cancela `ResetConfirm` y `ClearConfirm` solo en V4
- **V3:** `scripts/4_World/LFPG_SorterController.c:914-919` — `SelectOutput` solo hace
  `m_ResetConfirmActive = false` (no existe `ClearConfirm`).
- **V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:917-919` —
  `CancelResetConfirm(); CancelClearConfirm();` al cambiar de salida.
- **Qué cambia si borramos la V3:** la V4 no deja colgado un "CONFIRM?" al cambiar de pestaña.
  La V3 solo lo gestiona para ResetAll.
- **Dirección:** V4
- **Riesgo:** bajo

## D-09 — `BtnResetAll` y `BtnClearOut` se desarmutan mutuamente solo en V4
- **V3:** `scripts/4_World/LFPG_SorterController.c:1069-1081` — `BtnResetAll` no desarma
  ningún `ClearConfirm` (no existe).
- **V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:1163-1165` (`BtnResetAll` llama
  `CancelClearConfirm()`), `:1138` (`BtnClearOut` llama `CancelResetConfirm()`).
- **Qué cambia si borramos la V3:** la V4 evita dos confirmaciones rivales activas a la vez.
- **Dirección:** V4
- **Riesgo:** bajo

## D-10 — Mensajes de estado: V3 incluye pista accionable, V4 la omite
- **V3:** `scripts/4_World/LFPG_SorterController.c:809` (`"FAILED - REOPEN"`),
  `:834` (`"SORT FAILED"`), `:1151`/`:805` (`"SAVING..."`/`"SORTING..."` con puntos).
- **V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:798` (`"FAILED"`),
  `:819` (`"FAILED"`), `:1199`/`:1243` (`"SAVING"`/`"SORTING"` sin puntos), `:759` (`"NO POWER"`).
- **Qué cambia si borramos la V3:** la V4 pierde la pista "- REOPEN" (el usuario ya no sabe que
  debe reabrir el panel tras un fallo de save/sort). Añade "NO POWER" (ligado a D-03).
- **Dirección:** V3
- **Riesgo:** bajo

## D-11 — `BtnCatchAll`: label "CATCH-ALL" / "* CATCH-ALL" (V3) vs "CATCH-ALL SORTING: OFF/ON" (V4)
- **V3:** `scripts/4_World/LFPG_SorterController.c:1577-1586` —
  `label = "CATCH-ALL"` / `label = "* CATCH-ALL"`.
- **V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:1486-1496` —
  `label = "CATCH-ALL SORTING: OFF"` / `label = "CATCH-ALL SORTING: ON"`.
- **Qué cambia si borramos la V3:** etiqueta más explícita en V4. Cambio cosmético.
- **Dirección:** V4
- **Riesgo:** bajo

## D-12 — Colores de tipo de regla permutados entre V3 y V4
- **V3:** `scripts/4_World/LFPG_SorterController.c:1664-1669` —
  CAT=`COL_GREEN`, PFX=`COL_BLUE`, CON=`COL_AMBER`, SLT=`COL_PURPLE`.
- **V4:** `scripts/4_World/test/LFPG_SorterController_TEST.c:1577-1582` —
  CAT=`COL_BLUE`, PFX=`COL_AMBER`, CON=`COL_PURPLE`, SLT=`COL_GREEN`.
- **Qué cambia si borramos la V3:** la asignación color↔tipo cambia. Un usuario acostumbrado a
  V3 (categoría=verde) ve categoría=azul en V4. No es un bug, es una decisión de diseño S2, pero
  es una divergencia de comportamiento visual.
- **Dirección:** no decidible
- **Riesgo:** bajo

## D-13 — `LFPG_SorterTagView`: V4 añade `TagLeftBar` + `TagTypeLabel` y no tinta `TagBg`
- **V3:** `scripts/4_World/LFPG_SorterTagView.c:78-89` — tinta `TagBg` con
  `(color & 0x00FFFFFF) | 0x26000000`, texto en `COL_TEXT`. Sin barra lateral ni etiqueta de tipo.
- **V4:** `scripts/4_World/test/LFPG_SorterTagView_TEST.c:75-89` —
  `TagLeftBar.SetColor(color)` + `TagTypeLabel.SetText(typeTag)` + `TagTypeLabel.SetColor(color)`,
  texto en `COL_TEXT`. `TagBg` no recibe `SetColor` (lo que ponga el layout queda fijo).
- **Qué cambia si borramos la V3:** la V4 comunica el tipo de regla con una barra lateral
  coloreada + micro-etiqueta "CAT/PFX/CON/SLT", y deja el fondo del tag sin tinte dinámico.
- **Dirección:** no decidible
- **Riesgo:** bajo

## D-14 — Botones de footer `BtnSort` y `BtnClose` existen en V3, ausentes en V4
- **V3:** `gui/layouts/LFPG_Sorter.layout:2340` (`ButtonWidgetClass BtnSort`),
  `:2415` (`ButtonWidgetClass BtnClose`); dispatch en `LFPG_SorterView.c:969-970`
  (`UID_SORT`→`BtnSort`, `UID_CLOSE`→`BtnClose`).
- **V4:** ausentes. El layout V4 no declara `BtnSort` ni `BtnClose` (verificado con grep);
  `LFPG_SorterView_TEST.c` no define `UID_SORT` (504) ni `UID_CLOSE` (505) ni los dispatcha.
  Sort solo vía `BtnSortHeader`; close solo vía `BtnCloseX`.
- **Qué cambia si borramos la V3:** la V4 pierde dos puntos de entrada (sort y close en el
  footer). Es decisión de diseño S2, pero reduce redundancia de interacción.
- **Dirección:** V4
- **Riesgo:** bajo

---

## LO QUE NO PUDE VERIFICAR

- **Layouts completos:** no leí los `.layout` enteros (cada uno tiene ~2400-2800 líneas).
  Verifiqué por grep la presencia/ausencia de los botones relevantes (`BtnSort`, `BtnClose`,
  `TabOut0`, `TabRules`, `BtnPreview`, `OutputRow0`, `BuilderTabCategory`, etc.) y, para el
  layout V4, el uso de la propiedad `color` en `ImageWidgetClass` sin textura (líneas 22-80).
  No comparé geometría, posición ni el resto de widgets estáticos — el brief descarta el
  "layout distinto por sí mismo" como divergencia reportable.
- **`LFPG_MCP_SorterCmd.layout`:** referenciado por `LFPG_SorterView_TEST.c:1933` pero no lo leí.
  Su existencia/contenido afecta a D-04 (si el layout no existe, `EnsureMcpCmdWidget` cae en
  `m_McpCmdCreateFailed` y el hook queda inerte — el riesgo D-04 bajaría de medio a bajo).
- **Handler de servidor:** no leí `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` ni
  `LFPG_SorterLogic.c`. La divergencia D-14 (sub-ids RPC 60-67) asume que el servidor atiende
  esos ids; si no lo hace, la V4 no funciona en absoluto (no es una divergencia, es un
  prerrequisito). No lo verifiqué.
- **`LFPG_Sorter_TEST.c` (entity):** solo confirmé la herencia (`class LFPG_Sorter_TEST :
  LFPG_Sorter`) y que no redefine `LFPG_IsPowered`/`LFPG_IsLinked`. No leí el cuerpo completo;
  podría tener overrides de `SetActions` u otros métodos que cambien el comportamiento de la
  acción V4 (D-01) más allá de lo que muestra `LFPG_ActionOpenSorterPanel_TEST.c`.
- **`LFPG_UIScaler`:** no leí su implementación. D-02 asume que `ComputeScale`/`Apply`/`Capture`
  hacen lo que sus nombres sugieren; si la escala fuese no-op en el rango de resoluciones
  objetivo, el riesgo de D-02 bajaría.
- **`LFPG_PERFDIAG_ENABLED`:** la V4 lo usa en `Open` (`LFPG_SorterView_TEST.c:1349`) y en
  `SendPreviewNow`/`PopulatePreview` del controller V4. La V3 también lo usa en preview
  (`LFPG_SorterController.c:1826-1845`, `:1855-1871`). No verifiqué dónde se define ese flag
  ni su valor por defecto; si está apagado en release, D-05 y parte de D-04 son inertes en
  producción, pero `S1_PROBE` (D-05) NO está tras ese flag y dispara siempre.

