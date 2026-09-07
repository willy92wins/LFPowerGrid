# ENCARGO — verificar hallazgos de auditoría contra el código actual (SOLO LECTURA)

## Entorno
- Repo git en `P:\LFPowerGrid` (Enforce Script de DayZ, ficheros `.c`; **no es C**). Es tu workspace.
- Rama actual `sorter/v4-finish`, HEAD = `421cabb`. **Ese es el estado "actual" que debes verificar.**
- Las fichas de abajo son de una auditoría hecha sobre el commit `d61705e` (16-ago-2026). Desde entonces la rama lleva 22 commits.
- **PROHIBIDO**: editar cualquier fichero, `git commit`, `git checkout`, `git stash`, compilar, lanzar DayZ. Solo lectura, grep y `git diff`/`git show`.

## Método obligatorio
1. Los números de línea de las fichas son del commit viejo y **ya no valen**. Localiza el código por NOMBRE DE SÍMBOLO o por contenido (grep del nombre de función, variable o literal citado), **nunca** por número de línea.
2. Decide con el código que ves, no con lo que afirma la ficha. **Si la ficha describe mal el código actual, dilo.** No des el hallazgo por bueno.
3. Cita SIEMPRE `path:line` del árbol ACTUAL y pega el fragmento (3-10 líneas) que justifica tu veredicto.
4. Aviso de instrumento: hubo una normalización masiva de finales de línea, así que **todos** los diffs salen enormes y falsos. Usa siempre `git diff --ignore-all-space d61705e..HEAD -- <fichero>`.
5. Aviso de estructura: dos ficheros grandes se partieron en fachada + implementación. `scripts/4_World/LFPG_NetworkManager.c` y `scripts/4_World/LFPG_RPCServerHandler.c` conservan el nombre pero su lógica vive ahora en `scripts/5_Mission/LFPG_NetworkManagerImpl.c` y `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`. Si una ficha cita los de `4_World`, busca en los `Impl`.

## Veredictos (exactamente uno por ficha)
- `VIVO` — el patrón descrito sigue presente igual en HEAD.
- `CORREGIDO` — el código actual ya no tiene el defecto; di qué cambio lo arregló.
- `PARCIAL` — parte arreglada, parte no; di exactamente qué queda.
- `MOVIDO` — el código existe en otro fichero/símbolo; da la ubicación nueva y si sigue vivo allí.
- `NO_VERIFICABLE` — no se decide leyendo; di qué haría falta.

## Formato de salida (castellano)
Un bloque por ficha:

```
### <ID> — <VEREDICTO>
- **Dónde ahora:** path:line (árbol actual)
- **Qué veo:**
  <fragmento 3-10 líneas>
- **Razonamiento:** 2-4 frases.
- **Discrepancia con la ficha:** ninguna | <qué no cuadra>
```

## Cierre obligatorio
Termina con una sección `## LO QUE NO PUDE VERIFICAR`; si no hay nada, escribe "nada". No propongas arreglos. No edites nada.

---

# PARTE B — Censo de acoplamientos de la V3 del sorter (tan importante como la parte A)

El sorter tiene HOY dos implementaciones vivas en paralelo:
- **V3 producción:** `scripts/4_World/LFPG_SorterView.c`, `LFPG_SorterController.c`, `LFPG_SorterTagView.c`, `LFPG_SorterPreviewRow.c`
- **V4 nueva:** `scripts/4_World/test/LFPG_SorterView_TEST.c`, `LFPG_SorterController_TEST.c`, `LFPG_SorterTagView_TEST.c`, `LFPG_SorterPreviewRow_TEST.c`, `LFPG_Sorter_TEST.c`, `LFPG_ActionOpenSorterPanel_TEST.c`

El objetivo del proyecto es **jubilar la V3** para reducir el tamaño del mod. Se conocen tres bloqueadores, verificados en una sesión anterior; confírmalos o refútalos contra HEAD:
  (a) el sub-id RPC `SORTER_TEST_RESYNC = 65` no tiene emisor, y `SORTER_TEST_CARGO_REFRESH = 70` no tiene productor;
  (b) `LFPG_BTCAtmController.c` lee constantes `LFPG_SorterView.COL_*` de la clase V3;
  (c) hay guards que solo consultan `LFPG_SorterView.IsOpen()` (V3), en `LFPG_Actions.c` y `LFPG_ActionSyncSorter.c`.

Y además **busca todos los demás acoplamientos del mismo tipo**: recorre TODO el árbol buscando referencias a los símbolos V3 (`LFPG_SorterView`, `LFPG_SorterController`, `LFPG_ColorData`, `LFPG_SorterTagView`, `LFPG_SorterPreviewRow`) **desde ficheros que no sean los propios de la V3**, y lista cada una con `path:line`. Mira también `config.cpp` y los `.layout` de `gui/layouts/`.

Ese censo es el entregable más valioso de este encargo: dice cuánto trabajo real cuesta jubilar la V3. Entrégalo como tabla:

`símbolo V3 | fichero:línea que lo usa | qué necesita de la V3 | ¿bloquea el borrado? (sí/no) | sustituto V4 si existe`

---

# PARTE A — FICHAS A VERIFICAR (2)

#### S04

**El presupuesto global rompe el round-robin y puede privar de turno a otros sorters**

**Referencias al commit:** `scripts/3_Game/LFPG_Defines.c`: [L346–350](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_Defines.c#L346-L350) · `scripts/4_World/LFPG_NetworkManager.c`: [L6056–6135](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L6056-L6135), [L6181–6184](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L6181-L6184), [L6256–6260](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L6256-L6260).

**ALTA · CONFIRMADO · confianza alta · rendimiento/equidad.**
**Evidencia:** `scripts/3_Game/LFPG_Defines.c:346-350`; `scripts/4_World/LFPG_NetworkManager.c:6056-6135,6181-6184,6256-6260`.

Al agotar las 512 comprobaciones, se guarda `m_SorterCursor = sorterIndex`, no el siguiente sorter; el tick siguiente vuelve a empezar por ese mismo aparato. Con 48 reglas no coincidentes por objeto, ese sorter agota el presupuesto antes del máximo de 20 objetos. Se continúa recorriendo su contenedor en ticks consecutivos, retrasando a todos los siguientes. Ejemplo de modelo de operaciones: 1000 objetos×48 reglas requieren al menos 94 porciones de 512; el intervalo configurado es 5s. **No es un benchmark ni una predicción universal de 470s:** demuestra el tamaño de la cola posible sin carga de motor. Una entrada sostenida puede prolongar la privación.
**Cambio:** guardar el cursor local por sorter y rotar siempre la cola global después de una porción; cuota por sorter más presupuesto global, o ronda de fair scheduling con déficit. No eliminar la reanudación.
**Regresión:** sorter A lleno de objetos no coincidentes y B con uno coincidente; todos reciben turno acotado. Añadir objetos continuamente a A; eliminar/reordenar sorters; reiniciar configuración durante diferimiento.

#### S08

**Repack manual síncrono no está acotado por el presupuesto del scheduler**

**Referencias al commit:** `scripts/4_World/LFPG_SorterLogic.c`: [L1105–1306](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_SorterLogic.c#L1105-L1306), [L1339–1384](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_SorterLogic.c#L1339-L1384) · `scripts/4_World/LFPG_NetworkManager.c`: [L6452–6457](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L6452-L6457), [L6481](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L6481), [L6547](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L6547) · `scripts/4_World/LFPG_RPCServerHandler.c`: [L2605–2662](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L2605-L2662).

**ALTA · CONFIRMADO · confianza alta · rendimiento; riesgo de disponibilidad POTENCIAL.**
**Evidencia:** `scripts/4_World/LFPG_SorterLogic.c:1105-1306,1339-1384`; `scripts/4_World/LFPG_NetworkManager.c:6452-6457,6481,6547`; `scripts/4_World/LFPG_RPCServerHandler.c:2605-2662`.

El límite manual 200 afecta solo a la evaluación previa; RepackCargoInPlace procesa todos los N objetos y toda la cuadrícula. Ordena índices por inserción O(N²), busca rectángulos probando celdas, reintenta hasta N pasadas de N candidatos y construye **dos InventoryLocation por intento**, aunque fracase por ocupación. No hay límite de operaciones/tiempo de repack. El RPC tiene distancia y cooldown por jugador: no es remoto sin restricciones, pero varias solicitudes cercanas y cargos modded grandes requieren perfilado.
**Cambio:** sacar el repack a un trabajo incremental con límite de operaciones y una cola por contenedor; reusable source/destination locations; omitir objetos ya colocados; ordenación estable O(N log N) o por buckets de área si el rango real es pequeño. Revalidar ubicaciones al reanudar, cancelar ante cambios y nunca recuperar la ida al suelo. Medir la búsqueda de celdas: cambiar solo el sort no basta.
**Regresión:** cuadrícula casi llena, items grandes/rotados, bloqueo por ciclos, jugador modificando cargo, solicitud repetida, cierre CodeLock/VSM entre porciones; ni pérdida ni duplicación.