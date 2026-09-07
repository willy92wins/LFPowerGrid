Voy a ejecutar el encargo: verificación de 4 fichas (SEC20, R02, R04, R15) contra HEAD actual, solo lectura. Empiezo confirmando el estado del repo y localizando los símbolos.El hook bloqueó el comando encadenado. Pruebo con comandos simples:Localizo el handler FINISH_WIRING en el Impl y leo la zona crítica:Leo el inicio del handler (validaciones de longitud) y las reglas CanPreConnect:R02 confirmado en lo esencial. Verifico la constante de cull y el procesado de `m_TempKeys`:R15 confirmado en lo esencial. Verifico el segundo `DoExitCleanup` (línea 1273) y el handler del RPC de confirmación:Verifico el ReconcileTick que re-encola tras el TTL, para cerrar R04:Verificación completa. Redacto el informe final.# Verificación de fichas contra HEAD (`sorter/v4-finish`)

Nota de método: no pude confirmar el hash HEAD por shell (hooks rotos bloquean todo comando, ver cierre), pero el árbol leído es el workspace actual y la estructura fachada/Impl coincide con lo anunciado: `scripts/4_World/LFPG_RPCServerHandler.c` es solo `Dispatch` (línea 13) y la lógica vive en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`.

### SEC20 — VIVO
- **Dónde ahora:** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c:283` (longitud), `:465-496` (salto de HasPort/CanConnectTo), `:750-790` (difunde antes del grafo, sin reversión); `scripts/4_World/LFPG_IDevice.c:487-495` (GetDeviceId solo cast/reflexión); `scripts/5_Mission/LFPG_NetworkManagerImpl.c:869-911` (AddVanillaWire); `scripts/5_Mission/LFPG_ElecGraphImpl.c:1346-1363` (cap 12).
- **Qué veo:**

```5_Mission/LFPG_RPCServerHandlerImpl.c
283|        if (srcPort.Length() > 32 || dstPort.Length() > 32)
284|        {
285|            LFPG_Util.Warn("[FinishWiring-Server] denied (port too long)");
286|            return;
287|        }
...
465|        bool srcIsLFPG = (LFPG_DeviceAPI.GetDeviceId(srcObj) != "");
466|        bool dstIsLFPG = (LFPG_DeviceAPI.GetDeviceId(dstObj) != "");
467|
468|        if (srcIsLFPG)
469|        {
470|            if (!LFPG_DeviceAPI.HasPort(srcObj, srcPort, LFPG_PortDir.OUT))
```

```5_Mission/LFPG_RPCServerHandlerImpl.c
755|            LFPG_NetworkManager.Get().BroadcastVanillaWires(srcRealId, srcObj);
...
765|        bool edgeAdded = LFPG_NetworkManager.Get().NotifyGraphWireAdded(srcRealId, dstRealId, srcPort, dstPort, wd);
...
777|        if (!edgeAdded)
...
784|            LFPG_Util.Warn("[FinishWiring-Server] Graph edge not inserted (limit or missing node) — forcing rebuild");
785|            LFPG_NetworkManager.Get().PostBulkRebuildAndPropagate();
```

```4_World/LFPG_IDevice.c
487|    static string GetDeviceId(Object obj)
488|    {
489|        if (!obj)
490|            return "";
491|        LFPG_DeviceBase dev = LFPG_DeviceBase.Cast(obj);
492|        if (dev)
493|            return dev.LFPG_GetDeviceId();
494|        return CallString(obj, "LFPG_GetDeviceId", null, "");
495|    }
```

- **Razonamiento:** Respondiendo estrictamente a la pregunta del brief: **NO existe hoy validación del nombre de puerto en el servidor antes de mutar store/grafo**. Lo único que se comprueba es longitud ≤32 (líneas 283-287); `HasPort`/`CanConnectTo` se aplican solo si `GetDeviceId() != ""`, y `GetDeviceId` es exclusivamente cast a `LFPG_DeviceBase` o reflexión `LFPG_GetDeviceId`, así que un PowerGenerator/Spotlight vanilla sale por "" y se salta ambas. `CanPreConnect` (`LFPG_ConnectionRules.c:67-199`) valida dirección/cargo/waypoints/geometría pero nunca el nombre, y las direcciones OUT/IN las fija el propio handler en el parámetro (`:436`,`:441`), con lo que ese chequeo pasa siempre. `ValidateWire` (`LFPG_NetworkManagerImpl.c:1768-1816`) es solo geometría. El reemplazo por source-port (`:611`, `:644-650`) y por target-port (`:678-684`) casa por nombre exacto, y `AddVanillaWire` deduplica solo la tupla exacta (`:894-902`, normalizando únicamente `""`→`output_1` en `:873-875`). La difusión (`BroadcastVanillaWires`, `:756`) ocurre ANTES de `NotifyGraphWireAdded` (`:765`), y la rama `!edgeAdded` fuerza rebuild sin revertir el store (`:777-790`). Límites confirmados: grafo 12 (`LFPG_Defines.c:520`), store 64 por defecto con override `MaxWiresPerDevice` (`LFPG_Defines.c:18`, `LFPG_Settings.c:64`, `NetworkManagerImpl.c:884-892`), cliente 64 (`LFPG_RPCClientHandler.c:447`). La API de enumeración reconoce exactamente un `output_1`/`input_main` vanilla (`LFPG_IDevice.c:833-853`). No confundo esto con el límite configurable por dispositivo: ese cap existe pero no valida nombres.
- **Discrepancia con la ficha:** ninguna sustancial. Las rutas `4_World/LFPG_RPCServerHandler.c` y `4_World/LFPG_NetworkManager.c` que cita son hoy fachadas; la lógica está en los `Impl` de `5_Mission` (ya avisado en el brief). Todos los números de línea cambiaron, como se esperaba.

### R02 — VIVO
- **Dónde ahora:** `scripts/4_World/LFPG_CableRenderer.c:2444-2510` (rama owner null) y `:2556-2567` (recálculo de cachedMinDist).
- **Qué veo:**

```4_World/LFPG_CableRenderer.c
2473|                if (st.nullOwnerTicks >= 15)
2474|                {
...
2491|                        ref LFPG_WireSegmentInfo twInfo;
2492|                        if (m_WireSegments.Find(twKey, twInfo) && twInfo)
2493|                        {
2494|                            if (twInfo.cachedMinDist < LFPG_CULL_DISTANCE_M)
2495|                            {
2496|                                anyWireNearPlayer = true;
2497|                                break;
```

```4_World/LFPG_CableRenderer.c
2510|                continue; // Skip per-wire checks — entity is unavailable
...
2561|                float distToCenter = Math.Sqrt(distToCenterSq);
2562|                float closestEst = distToCenter - info.cachedRadius;
...
2567|                info.cachedMinDist = closestEst;
```

- **Razonamiento:** La rama `ownerObj == null` incrementa `nullOwnerTicks`, oculta los cables y sale con `continue` (línea 2510) ANTES del bucle por cable donde se recalcula `cachedMinDist` (línea 2567). Por tanto, la decisión de limpieza tras 15 ticks (línea 2494) usa el valor congelado de la última vez que el owner existió: si entonces estaba a <50 m (`LFPG_CULL_DISTANCE_M = 50.0`, `LFPG_Defines.c:550`), `anyWireNearPlayer` seguirá siendo true aunque el jugador se aleje, y el owner se retiene indefinidamente. No existe la función unificada `RemoveOwner` propuesta (grep sin resultados); la retirada sigue siendo el camino `m_TempKeys`→`m_GhostKeys` (`:2505`, `:2606-2612`).
- **Discrepancia con la ficha:** ninguna. Los números de línea cambiaron (CullTick ahora empieza en `:2354`), el mecanismo es idéntico.

### R04 — VIVO
- **Dónde ahora:** `scripts/4_World/LFPG_CableRenderer.c:2109` y `:2182-2193` (BuildOwnerWires), `:3858` y `:3992-4003` (RetryTick), `:4258` (único sitio que libera presupuesto, en DestroyWire); límite en `scripts/3_Game/LFPG_Defines.c:223`.
- **Qué veo:**

```4_World/LFPG_CableRenderer.c
2180|            string wireKey = ownerDeviceId + "|" + w.ToString();
2181|            int estSegs = EstimateSegments(m_TempPoints);
2182|            if (totalSegs + estSegs > LFPG_MAX_RENDERED_SEGS)
2183|            {
...
2189|                AddRetry(ownerDeviceId, w, LFPG_RetryReason.BUDGET);
```

```4_World/LFPG_CableRenderer.c
3991|            int estSegs = EstimateSegments(m_TempPoints);
3992|            if (totalSegs + estSegs > LFPG_MAX_RENDERED_SEGS)
3993|            {
3994|                // Budget exceeded. If this was a TARGET_MISSING entry whose target
3995|                // is now found, convert to BUDGET so it stops counting retries.
3996|                if (entry.reason == LFPG_RetryReason.TARGET_MISSING)
3997|                {
3998|                    entry.reason = LFPG_RetryReason.BUDGET;
```

- **Razonamiento:** `LFPG_MAX_RENDERED_SEGS = 512` (`LFPG_Defines.c:223`) sigue gobernando un presupuesto global único (`m_TotalSegCount`, `:781`). Tanto `BuildOwnerWires` como `RetryTick` conceden presupuesto en orden de llegada/cola sin comparar distancia ni visibilidad entre candidatos. `CullTick` solo conmuta `SetVisible` (`:2439`, `:2469`, `:2542`); nunca destruye geometría ni decrementa `m_TotalSegCount` — eso solo ocurre en `DestroyWire` (`:4258`). No existe desalojo de geometría lejana ni histéresis de presupuesto (los hits de "hysteresis" son de oclusión, no de budget). Consecuencia descrita por la ficha intacta: cables ocultos/retienidos consumen el presupuesto y los cercanos nuevos se rechazan.
- **Discrepancia con la ficha:** parcial en un síntoma secundario. La afirmación "RetryTick seguirá recalculando puntos/estimaciones de los rechazados [indefinidamente]" ya no es literal: desde v0.7.38 (M11) las entradas BUDGET expiran a los 60 s (`:3878-3885`, `LFPG_RETRY_BUDGET_TTL_S`). Pero `ReconcileTick` (`:4045-4084`) re-encola como TARGET_MISSING cualquier cable con datos y sin segmentos, que al reintentar vuelve a BUDGET (`:3996-3998`): el ciclo persiste en ráfagas de 60 s. El defecto central (reserva de segmentos invisibles, ausencia de prioridad espacial) no ha cambiado.

### R15 — VIVO
- **Dónde ahora:** `scripts/4_World/LFPG_CameraViewport.c:846-855` (timeout 5 s) y `:757-779` (DoExitCleanup con SetActive(false)); handler RPC en `scripts/4_World/LFPG_RPCClientHandler.c:149-156`.
- **Qué veo:**

```4_World/LFPG_CameraViewport.c
846|        // Timeout safety: if server never responds (5s), force cleanup.
847|        if (m_ExitPhase == 2)
848|        {
849|            m_ExitWaitTimer = m_ExitWaitTimer + timeslice;
850|            if (m_ExitWaitTimer >= 5.0)
851|            {
852|                LFPG_Util.Warn("[CameraViewport] Exit timeout — forcing cleanup");
853|                DoExitCleanup();
854|            }
855|        }
```

```4_World/LFPG_CameraViewport.c
771|        // Deactivate spectator camera only after SelectPlayer restored the player camera.
772|        if (m_ViewCamObj)
773|        {
774|            Camera viewCamTyped = Camera.Cast(m_ViewCamObj);
775|            if (viewCamTyped)
776|            {
777|                viewCamTyped.SetActive(false);
778|            }
779|            m_ViewCamObj = null;
```

```4_World/LFPG_RPCClientHandler.c
149|    static void HandleCCTVExitConfirm()
150|    {
151|        LFPG_CameraViewport vp = LFPG_CameraViewport.Get();
152|        if (vp)
153|        {
154|            vp.DoExitCleanup();
155|        }
156|    }
```

- **Razonamiento:** La FSM sigue exigiendo esperar `CCTV_EXIT_CONFIRM` (fase 2), pero a los 5 s llama al mismo `DoExitCleanup`, que ejecuta `SetActive(false)` sin verificar que el engine restauró la cámara del jugador — los propios comentarios (`:748-755`, `:770-771`, `:860-861`) vinculan esa transición prematura al crash de v1.3.1. Tampoco hay correlación de sesión: `HandleCCTVExitConfirm` invoca `DoExitCleanup()` sin leer ningún token del RPC, y el guard de idempotencia (`:759-761`) solo protege el estado ya limpio; una confirmación tardía llegando con una sesión NUEVA activa (`m_Active`/m_ViewCamObj` puestos) pasaría el guard y desactivaría la cámara nueva. El segundo `DoExitCleanup` (`:1273`) es un stub vacío de un `#else` de compilación, irrelevante. El crash en sí sigue sin ser confirmable sin DayZ, como la propia ficha declara (POTENCIAL, confianza media).
- **Discrepancia con la ficha:** ninguna sustancial; líneas desplazadas (timeout ahora en `:846-855`, DoExitCleanup en `:757-817`).

## LO QUE NO PUDE VERIFICAR
- **El `git diff --ignore-all-space d61705e..HEAD` que el brief sugiere como apoyo:** la herramienta de shell está completamente bloqueada por hooks de PowerShell rotos (error de sintaxis `&` en `launch-ledger.ps1`, `prime-agent-skills-gate.ps1`, `gpu-lease-gate.ps1`), así que no pude ejecutar ningún comando git. Los veredictos se apoyan exclusivamente en la lectura del árbol actual, que el brief declara como criterio decisorio; lo que no puedo afirmar es en qué commit concreto entraron los cambios observados (p. ej. el TTL M11 de R04, atribuido por comentarios a v0.7.38), ni confirmar que HEAD sea exactamente `421cabb`.
- **La reproducción dinámica de las cuatro fichas** (cliente modificado para SEC20, retención tras alejarse para R02, saturación de presupuesto para R04, confirmación demorada/duplicada para R15): el brief prohíbe lanzar DayZ, y R15 además es intrínsecamente no decidible leyendo (la propia ficha la marca POTENCIAL).