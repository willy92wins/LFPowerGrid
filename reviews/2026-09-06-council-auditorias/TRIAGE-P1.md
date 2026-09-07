# Triage de las 25 fichas P1 contra HEAD `421cabb` (rama `sorter/v4-finish`)

Método: (1) pre-filtro mecánico — intersección de los ficheros que cita cada ficha contra
`git diff --name-only --ignore-all-space d61705e..HEAD`; (2) lectura dirigida por símbolo
(nunca por número de línea) de las 19 fichas cuyos ficheros sí habían cambiado.

**Resultado: 24 VIVO + 1 PARCIAL. Cero corregidas.**

| ID | Veredicto | Dónde está hoy | Nota |
|---|---|---|---|
| G01 | VIVO (intacto) | `5_Mission/LFPG_ElecGraphImpl.c:3673+` | fichero sin un solo cambio desde `d61705e` |
| G02 | VIVO | `5_Mission/LFPG_ElecGraphImpl.c:3010`, `:3052` | solver suma `m_VirtualGeneration`, validador solo mira `incomingPower` |
| G04 | VIVO | `4_World/LFPG_VanillaActionOverrides.c:100`,`:172` | `RefreshSourceState` tiene un único llamador y ninguno parte de generador vanilla |
| G18 | VIVO (intacto) | `5_Mission/LFPG_ElecGraphImpl.c` | — |
| SEC01 | VIVO | `5_Mission/LFPG_NetworkManagerImpl.c:2150` y 7 sitios más | los 9 `.Send(` migraron idénticos; el FullSync valida identidad y luego envía `null` |
| SEC02 | VIVO | `5_Mission/LFPG_RPCServerHandlerImpl.c:602`,`:680` | `RemoveWiresTargeting` con `allowOthers=true` por defecto; el camino de corte SÍ aplica política — asimetría confirmada |
| SEC09 | **PARCIAL** | `3_Game/LFPG_FileUtil.c:580-585` (guarda nueva), `:214-218` (residuo) | arreglado en `ef29b73`; quedan dos vías: `.tmp` parseable sin target, y el último recurso documentado en `:280-283` |
| SEC20 | VIVO | `5_Mission/LFPG_RPCServerHandlerImpl.c:283`,`:465-496`,`:750-790` | **no hay validación del nombre de puerto en servidor antes de mutar**; solo longitud ≤32. `GetDeviceId()==""` en vanilla salta `HasPort`/`CanConnectTo`. Difunde antes de insertar en el grafo y no revierte |
| R02 | VIVO | `4_World/LFPG_CableRenderer.c:2444-2510`,`:2556-2567` | la rama owner-null hace `continue` antes de recalcular `cachedMinDist`; decide con distancia congelada |
| R04 | VIVO | `4_World/LFPG_CableRenderer.c:2182`,`:3992`; `3_Game/LFPG_Defines.c:223` | presupuesto global 512 sin prioridad espacial; `CullTick` nunca decrementa |
| R15 | VIVO | `4_World/LFPG_CameraViewport.c:846-855`,`:757-779` | timeout de 5 s llama al mismo cleanup sin confirmar restauración; sin token de sesión |
| S04 | VIVO | `5_Mission/LFPG_NetworkManagerImpl.c:6193-6197`,`:6272-6276` | al agotar presupuesto el cursor global se queda en el mismo sorter; hay reanudación local pero no rotación |
| S08 | VIVO | `5_Mission/LFPG_NetworkManagerImpl.c:6566`; `LFPG_SorterLogic.c:1105-1327` | `maxEval=200` no envuelve `RepackCargoInPlace`, que es O(N²) + hasta N pasadas. **V4 TEST hereda el mismo pico** (`RPCServerHandlerImpl.c:147-150`) |
| E02 | VIVO (intacto) | `5_Mission/LFPG_BTCHelper.c` | — |
| E03 | VIVO (intacto) | `5_Mission/LFPG_BTCHelper.c` | — |
| E04 | VIVO | `5_Mission/LFPG_BTCHelper.c:1465`,`:1505-1513` | bloque **byte a byte idéntico** al del commit auditado. BTC destruidos, crédito falla, se emite error y se sale sin restituir |
| E08 | VIVO (intacto) | — | — |
| E16 | VIVO | `5_Mission/LFPG_BalanceProvider_NativeImpl.c:1230-1243`; `4_World/LFPG_BTCAtm.c:165-176` | stock en RAM + tombstones borrados de forma durable, en distinto mecanismo y momento |
| D01 | VIVO | `4_World/LFPG_Furnace.c:453-470`,`:540-547` | deadline absoluto reescrito en cada encendido; `m_BurnNextMs` no se persiste |
| D02 | VIVO | `4_World/LFPG_Furnace.c:121-145`,`:289-292` | `super.EEInit()` activa el calor antes de que exista `m_UTSource`; el guard lo traga |
| D03 | VIVO | `4_World/LFPG_Battery.c:341-352`; `NetworkManagerImpl.c:7644`,`:7722` | round-trip float→int×10→float por tick; el adaptador no lo sufre → asimetría de precisión |
| D04 | VIVO | `4_World/LFPG_Intercom.c:822-831`; ausente en `config.cpp` | `LFPG_GhostPASBroadcaster` **no está declarada en CfgVehicles**; `config.cpp` es el único config del árbol y no tiene includes. El hermano `LFPG_GhostPASReceiver` sí está (`:2562`). Función muerta |
| D05 | VIVO | `4_World/LFPG_BatteryAdapter.c:84-89`; `lfpg_devicebase.c:430-455` | el comentario promete recogida con F, pero no reoverridea los tres guards heredados; y sin kit no hay desmontaje. Irrecuperable |
| D16 | VIVO (potencial) | `4_World/LFPG_DoorController.c:574-612`,`:754-804` | cero coincidencias de propiedad/permiso/candado en la ruta Fence |
| A03 | VIVO (intacto) | — | la cobertura actual valida el checker, no la lógica |

## Lo que el triage NO cubre
- **Las 126 fichas P2/P3 siguen sin triar una a una.** El pre-filtro mecánico dice que 28 de ellas
  viven en ficheros intactos (vivas por construcción) y 98 en ficheros tocados (desconocidas).
- Tres fichas necesitan ejecución para cerrarse: D16 (¿`Fence.OpenFence()` respeta el candado?),
  G02 (cuánto persiste la incoherencia), SEC01 (recuento de entregas por cliente).
- Dos lanes no pudieron correr `git` por el shell de su entorno; sus veredictos salen de lectura
  del árbol actual, que el encargo aceptaba como criterio decisorio.
