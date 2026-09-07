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

## Aviso específico de SEC20
Es el único hallazgo con carácter de vulnerabilidad de autoridad de servidor (un cliente modificado inventando nombres de puerto para saltarse el límite físico de cables). La propia auditoría avisa de que un hallazgo hermano (A06) resultó ser FALSO POSITIVO por confundir dos caminos distintos. Sé estricto: comprueba si HOY existe validación del nombre de puerto **en el lado servidor y ANTES de mutar** el grafo o el almacenamiento. No confundas ese camino con el límite configurable por dispositivo.

## Cierre obligatorio
Termina con una sección `## LO QUE NO PUDE VERIFICAR` con lo que quedó sin resolver y por qué; si no hay nada, escribe "nada". No propongas arreglos. No edites nada.

---

# FICHAS A VERIFICAR (4)

#### SEC20

**Los puertos inventados de dispositivos vanilla evitan el límite físico y provocan divergencia store/grafo**

**Referencias al commit:** `scripts/4_World/LFPG_RPCServerHandler.c`: [L259–283](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L259-L283), [L391–403](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L391-L403), [L428–442](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L428-L442), [L460–489](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L460-L489), [L632–676](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L632-L676), [L695–697](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L695-L697), [L750–781](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_RPCServerHandler.c#L750-L781) · `scripts/4_World/LFPG_IDevice.c`: [L96–107](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_IDevice.c#L96-L107), [L265–268](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_IDevice.c#L265-L268), [L386–394](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_IDevice.c#L386-L394), [L475–492](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_IDevice.c#L475-L492), [L804–835](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_IDevice.c#L804-L835) · `scripts/3_Game/LFPG_ConnectionRules.c`: [L85–175](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_ConnectionRules.c#L85-L175) · `scripts/4_World/LFPG_NetworkManager.c`: [L910–950](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L910-L950), [L1032–1039](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L1032-L1039), [L1160–1176](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L1160-L1176), [L1803–1849](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_NetworkManager.c#L1803-L1849) · `scripts/5_Mission/LFPG_ElecGraphImpl.c`: [L1339–1377](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/5_Mission/LFPG_ElecGraphImpl.c#L1339-L1377) · `scripts/3_Game/LFPG_Defines.c`: [L18](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_Defines.c#L18), [L215](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_Defines.c#L215), [L515](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_Defines.c#L515) · `scripts/3_Game/LFPG_Settings.c`: [L212–213](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_Settings.c#L212-L213), [L314–325](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_Settings.c#L314-L325).


**Alta · CONFIRMADO por flujo estático · Confianza alta; reproducción en DayZ pendiente · Validación de entrada, integridad y rendimiento.**

**Evidencia:** `scripts/4_World/LFPG_RPCServerHandler.c:259–283,391–403,428–442,460–489,632–676,695–697,750–781`; `scripts/4_World/LFPG_IDevice.c:96–107,265–268,386–394,475–492,804–835`; `scripts/3_Game/LFPG_ConnectionRules.c:85–175`; `scripts/4_World/LFPG_NetworkManager.c:910–950,1032–1039,1160–1176,1803–1849`; `scripts/5_Mission/LFPG_ElecGraphImpl.c:1339–1377`; límites `scripts/3_Game/LFPG_Defines.c:18,215,515`, `scripts/3_Game/LFPG_Settings.c:212–213,314–325`.

GetOrCreateDeviceId genera un ID determinista vanilla y lo registra, pero GetDeviceId consulta exclusivamente método/cast LFPG: para PowerGenerator/Spotlight sin extensión LFPG continúa vacío. Por ello `srcIsLFPG`/`dstIsLFPG` son false y FINISH_WIRING omite HasPort y CanConnectTo. El servidor comprueba solo longitud de los nombres, fija las direcciones OUT/IN en el parámetro de prevalidación y ValidateWire solo valida geometría. La API de enumeración, en cambio, reconoce exactamente un `output_1` y un `input_main` vanilla.

**Escenario de pruebas:** jugador autenticado con carrete, PowerGenerator y Spotlight colocados en el mundo, próximos, sin ciclos y dentro de límites geométricos/cooldown/cuota. Un cliente modificado presenta nombres cortos distintos para ambos puertos. El nombre evita el reemplazo por source-port y el reverse-index por target-port, y AddVanillaWire solo deduplica la tupla exacta. Se almacenan conexiones paralelas imposibles desde la UI. Hasta 12 pueden entrar al grafo; la 13.ª se almacena y difunde antes de que AddEdgeInternal rechace el límite 12. La rama de fallo fuerza rebuild global pero no revierte el store. Se puede alcanzar el cap de almacenamiento 64 por defecto, o 128 si el administrador eleva MaxWiresPerDevice; en este último caso se supera además el cap cliente 64. La UI ordinaria no produce ese estado; requiere cliente modificado y acceso físico a estos dispositivos, no permisos administrativos. No se afirma un bypass del cooldown ni crecimiento ilimitado.

**Cambio:** validación y normalización autoritativa de puertos para **todos** los dispositivos antes de registrar/mutar: vanilla acepta solamente sus puertos canónicos (normalizar vacíos heredados únicamente si esa compatibilidad se mantiene); extensiones deben declarar puertos explícitos. Comprobar capacidad proyectada del grafo antes de modificar el store y no difundir un wire que el grafo no admite. Reutilizar el plan de SEC03. Alinear límites store/grafo/cliente según contrato, sin usar subirlos todos como arreglo a la ausencia de validación.

**Regresión:** vanilla normal y mod compat siguen cableando por sus puertos reales; nombres inexistentes, vacíos/no canónicos y tuplas paralelas se rechazan sin cambios ni broadcast/rebuild. Caso límite 12 y fallo de AddEdge conservan stores, contadores y grafo consistentes; comparar coste y número de rebuilds de operaciones rechazadas en el servidor de pruebas.

#### R02

**La limpieza de owners desaparecidos decide con una distancia congelada**

**Referencias al commit:** `scripts/4_World/LFPG_CableRenderer.c`: [L2420–2489](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CableRenderer.c#L2420-L2489), [L2536–2541](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CableRenderer.c#L2536-L2541).


**ALTA · CONFIRMADO · Confianza ALTA.**

**Ubicación:** `scripts/4_World/LFPG_CableRenderer.c:2420–2489`; `scripts/4_World/LFPG_CableRenderer.c:2536–2541`.

**Evidencia:** Cuando ownerObj es null, CullTick continúa antes del cálculo de cachedMinDist. Tras 15 ticks usa justamente twInfo.cachedMinDist<50 para decidir conservar al owner. Si la última distancia fue cercana, seguirá cercana aunque el jugador se aleje.

**Condición/impacto:** Retención indefinida durante esa sesión de owner, JSON, geometría y presupuesto de segmentos de un objeto eliminado o descargado. Cada CullTick y DrawFrame seguirá recorriendo las entradas retenidas. El límite de 512 segmentos evita crecimiento ilimitado de geometría, pero hace más grave el bloqueo de nuevos cables.

**Cambio propuesto:** Calcular la distancia actual al bounding sphere con pp para la decisión de limpieza; separar grace period de streaming de TTL de datos y de TTL de geometría. Retirar índices y reintentos mediante una sola función RemoveOwner.

**Comprobación de regresión:** Construir junto al jugador; eliminar/descargar owner; alejarse más de 50m; tras el grace period comprobar reducción de owners/segmentos y que al regresar la resincronización reconstruye el cable.

#### R04

**El presupuesto global reserva segmentos invisibles y no prioriza cables próximos**

**Referencias al commit:** `scripts/4_World/LFPG_CableRenderer.c`: [L2090–2176](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CableRenderer.c#L2090-L2176), [L2400–2418](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CableRenderer.c#L2400-L2418), [L3960–3977](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CableRenderer.c#L3960-L3977), [L4214–4230](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CableRenderer.c#L4214-L4230) · `scripts/3_Game/LFPG_Defines.c`: [L218](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_Defines.c#L218).


**ALTA · CONFIRMADO · Confianza ALTA.**

**Ubicación:** `scripts/4_World/LFPG_CableRenderer.c:2090–2176`; `scripts/4_World/LFPG_CableRenderer.c:2400–2418`; `scripts/4_World/LFPG_CableRenderer.c:3960–3977`; `scripts/4_World/LFPG_CableRenderer.c:4214–4230`; `scripts/3_Game/LFPG_Defines.c:218–218`.

**Evidencia:** m_TotalSegCount limita geometría construida a 512. CullTick sólo cambia visible; no libera ni reduce la geometría distante. BuildOwnerWires y RetryTick conceden presupuesto por orden de llegada/cola, sin comparar prioridad espacial.

**Condición/impacto:** Tras visitar bases densas, cables nuevos junto al jugador pueden no construirse porque cables ocultos todavía cargados o retenidos consumen todo el presupuesto. RetryTick seguirá recalculando puntos/estimaciones de los rechazados. No basta con incrementar el límite.

**Cambio propuesto:** Separar topología conocida de geometría residente. Asignar presupuesto por proximidad/visibilidad con histéresis, degradar a una representación económica o desalojar geometría lejana antes de denegar la cercana. Despertar reintentos BUDGET al liberar presupuesto.

**Comprobación de regresión:** Llenar presupuesto en zona A; desplazarse a B con A aún cargada; comprobar aparición prioritaria en B, recuperación al volver y estabilidad cuando el jugador está en el borde.

#### R15

**El timeout de salida CCTV desactiva la cámara sin confirmar la restauración del jugador**

**Referencias al commit:** `scripts/4_World/LFPG_CameraViewport.c`: [L846–855](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CameraViewport.c#L846-L855), [L772–787](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_CameraViewport.c#L772-L787).


**ALTA · POTENCIAL · Confianza MEDIA.**

**Ubicación:** `scripts/4_World/LFPG_CameraViewport.c:846–855`; `scripts/4_World/LFPG_CameraViewport.c:772–787`.

**Evidencia:** La propia FSM exige esperar CCTV_EXIT_CONFIRM tras SelectPlayer para desactivar la cámara; sin embargo al llegar a 5s llama DoExitCleanup, que ejecuta SetActive(false) aun si no llegó confirmación ni se verificó que el engine restauró player.

**Condición/impacto:** POTENCIAL: con servidor bloqueado/latencia prolongada se ejecuta precisamente la transición que los comentarios del proyecto asocian a crashes de cámara. La auditoría no puede confirmar un crash nativo sin DayZ.

**Cambio propuesto:** Timeout debe reintentar/salir a un estado degradado que conserve cámara válida, o verificar player/cámara restaurados antes de desactivar. Añadir correlación de sesión a confirmaciones para que una tardía no cierre una sesión nueva. Coordinar con timeout autoritativo y desconexión.

**Comprobación de regresión:** Demorar confirmación más de 5s, duplicarla, desconectar servidor, morir durante la espera y volver a entrar cuando llegue confirmación tardía.