<!-- Extraido del stream de cursor-agent: --mode ask no puede escribir en el workspace -->
<!-- modelo servido: Cursor Grok 4.6 Extra High | parada: success | coste: None | llamadas a herramienta: 192 -->
Voy a leer `BRIEF-REVIEW.md` y seguirlo de principio a fin. Primero cargo el brief y el contexto de revisión.El brief de revisión pide dictamen solo lectura. Cargo el encargo original, el informe y el diff.Voy a contrastar el diff con el código V3, el scaler y los RPC, no solo con el informe.El brief pide un dictamen por escrito y no tocar código. Estoy en modo Ask, así que no puedo crear `DICTAMEN.md` en disco; abajo va el dictamen completo para que lo copies o pases a Agent mode.

---

**VERDE — 0 GRAVE / 1 MEDIO / 1 MENOR**

El arreglo cierra las seis fichas de código. S10 no aparece en esta base como divergencia de preview. El informe no miente en las citas que abrí. El diff se queda en la lista blanca.

### MEDIO — `RefreshCargoPreview` vacía el preview y luego puede pintar un RPC ya en vuelo

**Dónde:** `scripts/4_World/test/LFPG_SorterController_TEST.c:1709` (el `Clear`) y `:1768` (`PopulatePreview`).

**Qué está mal:** Un aviso de cargo pone `PreviewItems` a cero y llama a `RequestPreview()`. Esa llamada solo arma el debounce; no cancela el in-flight. `PopulatePreview` solo descarta si cambió la salida (`outputIdx != m_SelectedOutput`). Si había un preview en vuelo, esa respuesta rellena la lista con el cargo viejo; el debounce manda otro RPC después.

**Por qué importa:** El usuario puede ver vacío → datos viejos → datos nuevos. El in-flight/debounce sí agrupa ráfagas, pero no hay generación ni “descartar al invalidar”. No deja el camino de error peor que antes (antes la V4 no se enteraba del cargo); el refresco queda a medias un instante.

**Qué habría que hacer:** Invalidar con un número de generación (o ignorar la respuesta in-flight tras un refresh de cargo) y no pintar filas hasta la petición nueva; o no hacer `Clear()` hasta esa respuesta.

### MENOR — Capture V4 sin la guarda de `SorterPanel` que sí tiene la V3

**Dónde:** `scripts/4_World/test/LFPG_SorterView_TEST.c:1384`. Contraste: `scripts/4_World/LFPG_SorterView.c:1197`.

**Qué está mal:** La V3 solo llama a `Capture` si `SorterPanel` existe. La V4 hace `CapturePanel(s_Instance.SorterPanel)` siempre. `LFPG_UIScaler.Capture` retorna si el root es null (`scripts/3_Game/LFPG_UIScaler.c:65-66`), así que no pisa la captura V3 (el swap restaura el estado previo), pero si el binding falla la V4 queda sin captura y `ApplyPanel` no hace nada, sin el `if` ni un log propio.

**Por qué importa:** Mismo fallo silencioso de “panel sin escala”, un poco más opaco que en V3.

**Qué habría que hacer:** No llamar a `CapturePanel` si `SorterPanel` es null, igual que la V3, y dejar rastro si el binding faltó.

---

## Tabla ficha por ficha

| Ficha | Implementador | Revisor |
|---|---|---|
| 1. D-02 UIScaler en V4 | ARREGLADA | **ARREGLADA** |
| 2. D-01 guardas de apertura | ARREGLADA | **ARREGLADA** |
| 3. Gate `S1_PROBE` | ARREGLADA | **ARREGLADA** |
| 4. Hook MCP fuera de retail | ARREGLADA | **ARREGLADA** |
| 5. RPC 65 + consumo de cargo | ARREGLADA | **ARREGLADA** |
| 6. `IsOpen()` V3+V4 | ARREGLADA | **ARREGLADA** |
| S10 protección de preview | NO-LOCALIZADA | **NO-LOCALIZADA** |

### D-02
Capture en `LFPG_SorterView_TEST.c:1384`, Apply antes de centrar en `:1590-1594`, Reset en `:1506`. Tags en `LFPG_SorterTagView_TEST.c:121-128` y filas en `LFPG_SorterPreviewRow_TEST.c:81-86`, con `m_Scaled` y `ScaleWidget`, como V3. El contexto `LFPG_SorterScaleContext_TEST` (`:83-138`) no es el copy-paste literal del brief: `Capture()` vacía los estáticos (`LFPG_UIScaler.c:78-83`). Sin aislamiento, el primer Init V4 borraría la captura V3. Swap de los cinco arrays y los dos flags, Apply/Reset, y restauración: coherente. Init llama a `EnsureViewBindings` antes de capturar (`:1382-1384`), necesario porque Dabs a veces no rellena `SorterPanel`.

### D-01
`GetType()` exacto, `LFPG_IsPowered()` y `LFPG_IsLinked()` en `LFPG_ActionOpenSorterPanel_TEST.c:51`, `:58`, `:73`; mismo `GetType` al emitir en `:94`. Orden alineado con `LFPG_ActionOpenSorterPanel.c:56`, `:64`, `:80`. Mutex V3/V4 intacto (`:67-70`). `LFPG_IsPowered` / `LFPG_IsLinked` salen de SyncVars (`LFPG_Sorter.c:94-97`, `:165`, `:621`): `ActionCondition` puede leerlos en cliente y servidor.

### S1_PROBE
Constanta `S1_PROBE` eliminada. La sonda cuelga de `LFPG_PERFDIAG_ENABLED` en `LFPG_SorterView_TEST.c:1658`. El flag está en `LFPG_Defines.c:405` (`false`). El cuerpo (`RunS1Probe`, `:1669`) se conserva, `Print` incluidos: el brief pedía gatear, no reescribir. Distinto del MCP: aquí no pedían sacar el código del binario.

### MCP
`#ifdef DIAG_DEVELOPER` como en `LFPG_FaultInject.c:17`. Miembros `:177`, poll `:438`, ctor `:458`, dtor `:470`, open `:1630`, close `:1752`, bloque `:1994-2255`. Auxiliares del controller `:2256-2290`. En retail no quedan identificadores MCP. El layout `LFPG_MCP_SorterCmd.layout` no se toca.

### RPC 65 y cargo
Emisor en `LFPG_ActionSyncSorter.c:104-108`: `GetType() == "LFPG_Sorter_TEST"` → 65; si no, 29. Payload `subId`, netLow, netHigh, el mismo que lee `HandleSorterResync` (`LFPG_RPCServerHandlerImpl.c:2709-2722`). La entidad V4 hereda `AddAction(LFPG_ActionSyncSorter)` (`LFPG_Sorter.c:112`) y no sustituye esa acción (`LFPG_Sorter_TEST.c:39-43`). ACK 66 ya existía (`LFPG_RPCClientHandler.c:79`, `:984`).

Consumo de cargo: suscripción en open `:1654-1656`, baja en close `:1750-1751` y Cleanup `:1502-1503`. Callback `:1520-1528` → `RefreshCargoPreview` `:1706`. El 70 no se envía desde el cliente. El servidor publica 34 (`LFPG_NetworkManagerImpl.c:6316`); 34, 70 y ACK 69 llaman a `LFPG_CargoRefreshSignal.Request()` (`LFPG_RPCClientHandler.c:603`, `:612`, `:972`, `:976`). `LFPG_RPCClientHandler.c` está en la blanca y no hacía falta tocarlo.

### IsOpen
`LFPG_Actions.c:530` y `LFPG_ActionSyncSorter.c:71`, dentro de `#ifndef SERVER`, sin quitar la consulta V3. `LFPG_MissionInit.c` ya miraba las dos (fuera de alcance, y bien).

### S10
`RequestPreview` / `SendPreviewNow` / `PopulatePreview`: misma política (par, debounce, in-flight, descarte por salida). V3 `:1779`, `:1800`, `:1847`; V4 `:1718`, `:1740`, `:1768`. La diferencia real es edición: V4 `CanEdit() = paired && powered` (`:934`) y `RefreshPowerState` (`:960`); V3 no tiene `m_IsPowered` y muta solo con `m_IsPaired`. El comentario V4 (`:932-933`) deja preview permitido sin energía. No hay una guarda de preview más estricta que portar. NO-LOCALIZADA es el veredicto correcto; inventar un corte por energía en preview habría sido política nueva.

---

## Alcance, convenciones, informe

**Lista blanca:** `CAMBIOS.diff` toca solo los siete `.c` permitidos. `LFPG_RPCClientHandler.c` no se modificó. `INFORME.md` es el entregable, no un choque con otras lanes.

**Convenciones en líneas añadidas:** sin `? :`, `++`, `--`, `+=`, `foreach`, `Print(` ni `ref` en params/locales/retornos. Los `ref` nuevos son miembros (`m_Widgets`, `m_UIScaler`). Indentación nueva con tabs; no hay reformateo masivo ni cambio de CRLF que infle el diff.

**Honestidad del informe:** las citas `path:line` que abrí coinciden (Capture `:1384`, PERFDIAG `:1658`, MCP `:1994`, emisor `:104`, `IsOpen` `:530`/`:71`, S10). `LO QUE NO PUDE VERIFICAR` es real (compile, DPI, RPC, Dabs, diag vs retail, sentido histórico de S10), no relleno. No declara que compile ni que esté listo para publicar.

---

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

El brief pide copiar el patrón V3 de `LFPG_UIScaler` “tal cual”. Eso no convive con dos paneles: `Capture()` es global y `Clear()` de los arrays (`LFPG_UIScaler.c:78-83`). El Isolation en un fichero de la blanca es la opción conservadora; la copia literal habría roto la V3. `LFPG_UIScaler.c` además encaja el panel a 720×590 (`:171-173`); la V4 aún documenta un resize a 820×600. Portar el scaler no hace la V4 “apta para cualquier DPI”.

El 70 no es un RPC que el cliente deba emitir. En esta base el productor común manda 34. Pedir “cablear el lado cliente de 65 y 70” mezcla un emisor que sí faltaba (65) con una notificación que ya tenía receptor. Meter un productor 70 exigiría `LFPG_NetworkManagerImpl.c`, fuera de blanca.

S10 da por hecho que V3 y V4 “divergen en protección de preview”. En `8de29d5` no. La deriva está en edición con energía (solo V4). Aplicar “la más estricta” a preview habría inventado un corte que ninguna de las dos aplica. La ficha original (council 2026-09-06) no describe el mecanismo; o se cerró en un merge posterior a `d61705e`, o confundieron preview con `CanEdit`.

“Dejar la V4 apta para producción” choca con el propio brief: no hay compile, no hay juego, y la V3 no se borra. Lo que hay es paridad de guardas, scaler aislado, diagnósticos gateados y el emisor 65. La aptitud de producción sigue siendo una prueba in-game.

---

## LO QUE NO PUDE COMPROBAR

- Compilación Enforce al cargar el mundo: acceso del subclase a `protected static` de `LFPG_UIScaler` en `SwapState` (`LFPG_SorterView_TEST.c:116-136`); `ScriptInvoker.Insert(OnCargoRefresh)` con método estático (`:1656`); equilibrio real de `#ifdef DIAG_DEVELOPER` en diag vs retail vs `#ifndef SERVER`.
- Escalado, DPI, captura con widgets aún no mostrados (V4 captura en `:1384` y hace `Show(false)` en `:1388`; la V3 oculta antes de capturar).
- Entrega de RPC 65/66, refresco 34/70/69 y flicker del MEDIO con dos clientes.
- Que `DIAG_DEVELOPER` esté definido exactamente en el binario publicado que usa el equipo (el patrón del repo es DayZDiag, `LFPG_FaultInject.c:5-7`).
- Las corridas de `script_validator.py` / `ui_reconcile.py` que cita el informe; no las repetí.
- El texto original de S10 más allá de “divergen en protección de preview”; en este árbol no hay ficha con el mecanismo.
- Qué esté haciendo en paralelo la lane de `LFPG_Defines.c` con `LFPG_PERFDIAG_ENABLED`.

---

No pude dejar `DICTAMEN.md` en la raíz: esta sesión está en Ask mode y no escribe ficheros. Si cambias a Agent mode y pides “escribe DICTAMEN.md”, lo dejo en disco con este mismo contenido.