# PLAN DEFINITIVO — LFPowerGrid

Consolidado del council de 2026-09-08. Arbitro: Claude (orquestador).
Manifiesto: `MANIFEST.md` · brief_sha `abed2152…` · 5 lanes `COMPLETE`, 1 `QUOTA`.

---

## 0. EL VEREDICTO, EN CINCO LINEAS

1. **El backlog NO son ~170 items.** De los 46 apuntes de la auditoria, solo **~7 son codigo
   nuevo y barato**. El resto ya esta cubierto, hecho, refutado, o es una decision del dueño.
2. **La auditoria esta medida contra un arbol que ya no existe**: se escribio sobre `a187898`,
   y `main` lleva desde entonces **28 commits, 48 ficheros, +1.978 / −5.307 lineas**.
3. **La pregunta de producto que yo puse en el brief estaba mal.** `_TEST` no puede salir del
   release porque **`_TEST` ES la UI del sorter** desde T0b. Las cinco lanes lo vieron.
4. **No se parte `LFPG_NetworkManagerImpl.c`.** 4 lanes contra 1, y mi censo lo zanja: solo
   **20 de las 92 fichas** viven en los dos god-files. El **78 %** del trabajo esta fuera.
5. **El techo de paralelismo no es git: es la revision.** Por ficheros caben 8-13 lanes; por
   revisor caben 4. Ese es el numero real.

---

## 1. LO QUE EL COUNCIL CAMBIO, Y YO VERIFIQUE

Nada de esta tabla es opinion de lane: la re-abri yo antes de darle peso (`C15`).

| afirmacion | quien la trajo | verificacion del orquestador |
|---|---|---|
| `_TEST` es la unica UI del sorter | grok, kimik3, fable, astra | ✅ `scripts/4_World/LFPG_Sorter.c:111` engancha solo `LFPG_ActionOpenSorterPanel_TEST`; `LFPG_ActionRegistration.c:68` idem. La V3 no esta registrada |
| `S1_PROBE` ya no existe | grok | ✅ **cero apariciones** en todo `scripts/`. El D-05 del HANDOFF esta muerto |
| La V4 si lleva `LFPG_UIScaler` | grok | ✅ `test/LFPG_SorterView_TEST.c:83` lo extiende, `:96` Capture, `:103` Apply; tambien `SorterPreviewRow_TEST.c:84` y `SorterTagView_TEST.c:123`. El D-02 del HANDOFF esta muerto |
| El dios son 7.810 lineas, no 6.989 | glm, kimik3, astra | ✅ `wc -l` = **7810**. La cifra de la auditoria esta desfasada |
| `RPCServerHandlerImpl` es el 2º cuello | kimik3 | ✅ **3.088** lineas |
| La auditoria mide un arbol viejo | kimik3 | ✅ `git log a187898..main` = **28 commits**; `git diff --stat` = 48 ficheros, +1.978 / −5.307 |
| `LFPG_Sorter_TEST` y su Kit son `scope = 2` | glm, kimik3, fable | ✅ `config.cpp:1080-1091`. Spawneables ⇒ persistencia real |
| **«23 fichas viven en el dios»** | glm | ❌ **RECHAZADO-FALSO.** Son **13**. Metio fichas de `SorterLogic`, `RPCServerHandlerImpl`, `ControlSessionRegistry` y `CableRenderer` |

### El censo que zanja el reparto

Las 92 fichas VIVA caen en **37 ficheros** (censo completo en `CENSO-REPARTO-ORQUESTADOR.md`):

| n | fichero |
|---|---|
| 13 | `scripts/5_Mission/LFPG_NetworkManagerImpl.c` |
| 13 | `scripts/4_World/LFPG_CableRenderer.c` ← **nadie lo habia señalado** |
| 8 | `scripts/5_Mission/LFPG_ElecGraphImpl.c` |
| 7 | `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` |
| ≤3 | los otros 33 ficheros |

**20 de 92 en los dos god-files. El 78 % esta fuera y es paralelizable hoy, sin refactor.**

---

## 2. DISENSO FUERTE — verbatim, sin resumir

La unica discrepancia real del council. Se copia literal porque decide el plan entero.

> **`glm`** — «**Mi recomendación: partir S1 primero (lane `P`), luego repartir.** Sin S1, el
> plan tiene una lane de 7.810 líneas que serializa todo el servidor. Con S1, el plan gana 4-5
> lanes paralelas al coste de un refactor estructural que la auditoría ya priorizó.»

> **`grok`** — «**Partir `LFPG_NetworkManagerImpl.c` no es el primer movimiento, es el que
> impide el primero.** […] Extraerlo por dominio (S1) es un refactor de ~7.000 líneas que no
> cierra G06 ni S15 y concentra el riesgo sobre jugadores reales. Las fichas de ese fichero se
> parchean **en serie, in situ**. El split, si alguna vez, es *después* de que esas fichas estén
> verdes, no la condición para empezar.»

> **`astra`** — «Un propietario puede ejecutar cambios cortos en secuencia mientras otros
> trabajan fuera. Extraer ahora trasladaría el conflicto a fachadas, callbacks y referencias sin
> cerrar primero las regresiones.»

> **`kimik3`** — «El plan que rinde no es "repartir con cuidado", es **congelar ambos en la
> oleada 1** (el 60% del trabajo vive fuera de ellos) y serializarlos después.»

**Arbitraje: gana la posicion de `kimik3`, 4 lanes contra 1.** Y no por mayoria — por evidencia:
el censo mecanico muestra que el trabajo fuera de los god-files es el **78 %**, no el 60 % que
estimo `kimik3`, asi que su propia tesis sale reforzada por un margen mayor del que pidio. La
premisa de `glm` («sin partir no hay paralelismo») es falsa contra el censo.

---

## 3. LA RESPUESTA A LA PREGUNTA: cuanto de la auditoria es nuevo

Las dos lanes mas cuidadosas dan recuentos distintos. **La dispersion se publica, no se
promedia** (`C17`):

| lane | cubierto/hecho/refutado | codigo nuevo | perfil sin medir | decision del dueño | higiene |
|---|---|---|---|---|---|
| `fable` | 24 | **7** | 7 | 6 | 2 |
| `astra` | 20 (4 + 11 parcial + 5 sin accion) | \| 26 «no cubiertas», que el mismo desglosa en decisiones, refactors sin medida y solapes internos (`H6/U1`, `H5/U5`) | | |

Normalizados, no estan lejos: `fable` suma 22 «nuevos» de todas las clases, `astra` 26. **Lo que
las dos afirman igual, y es lo que importa: el codigo nuevo, barato y accionable son ~7 items.**

**Y una parte de la auditoria esta MUERTA, no cubierta** — murio esta misma mañana:

- **S2** (fork TEST) — lo mato T0b (`9fdafa1`).
- **§6.2** (`S1_PROBE` suelto, `UIScaler` ausente en V4) — lo mato `l5`. Verificado arriba.
- **S6** (migradores no-op) — lo mato `l8` / M-03.
- **H9** — lo mato S08.

⚠ **Aviso de `astra` que hay que respetar al ejecutar:** tres recetas de la auditoria
**cambiarian comportamiento** si se aplican literales — el early-return de agua se saltaria el
apagado de aspersores (`NM:5492`), el ancho de cable depende de la profundidad por segmento
(`CableRenderer:3400`), y la raiz del culling produce metros que se consumen despues
(`CableRenderer:2510`). **No portar esas tres tal cual.**

⚠ **Aviso de `kimik3` sobre la particion:** disjunto por fichero es necesario pero **no
suficiente**. Hay unidades de cambio que cruzan dos ficheros con dueños distintos — el ACK
cliente/servidor de `S11/S12`, `G06` entre manager y grafo, `D13` entre controller y planner,
`R27` HUD/dispatch, `S06` semantica L-E con codigo en NM. **Cada una de esas lanes nombra su
contrato de interfaz en el brief**, o la particion solo mueve el conflicto del merge al diseño.

---

## 4. DECISIONES QUE SON TUYAS — el plan no arranca sin estas

Ninguna se puede resolver leyendo el arbol.

### D1 · El journal del dinero — **la que mas cambia el tamaño**
Tres lanes (`grok`, `astra`, `fable`) coinciden en que la decision grande no era `_TEST` sino
esta. Los **7 CONFLICTO** estan todos en el camino del saldo y chocan con la reescritura de T1.
Efectos sobre saldo, inventario y stock siguen separados (`LFPG_BTCHelper.c:2192`, `:2383`,
`:2582`, `:2790`); cambiarles el orden **no demuestra atomicidad**.

- **(a) Aceptar como riesgo**, igual que se hizo con E04 → salen **7 items gordos** del plan.
- **(b) Cierre atomico con journal** → **añade** un tramo largo y exige armar el inyector T5,
  que nunca se ha ejecutado.

### D2 · T0c (renombrar `_TEST` → canonico)
El council lo da por **cancelable**: efecto ±0 fichas, y renombrar un classname `scope = 2`
puede romper bases de jugadores. Para retirarlo del backlog hace falta tu firma.

### D3 · El PBO desplegado
`P:\Mods` tiene el build de prueba (`8d558ae0…`), no produccion (`98ceb1eb…`, respaldada en
`_staging`). No afecta a los jugadores — su servidor va por el Workshop — pero el proximo que
arranque aqui mide sobre codigo nuevo creyendo que mide produccion. ¿Restaurar o promover?

### D4 · `archive/t2-autoridad-servidor`
`grok` sostiene que **no se fusiona**: `main` ya tiene T2 por la lane `l4`, y traer aquellas
1.162 lineas reintroduciria un paralelo historico. Lo que queda de T2 es in-game y dos MEDIO
residuales, no ese codigo. ¿Se archiva la rama definitivamente?

### D5 · Los 7 «NUEVO-perfil» de la auditoria
Optimizaciones **sin una sola medida** detras. La invariante del proyecto dice que recortar sin
medir no rinde. Propongo dejarlas fuera hasta que exista un numero.

---

## 5. EL PLAN

**Regla que gobierna todo:** los dos god-files (`LFPG_NetworkManagerImpl.c`,
`LFPG_RPCServerHandlerImpl.c`) **estan congelados en la oleada 1**. Nadie los abre. Se
serializan en la oleada 2, un solo dueño cada uno.

### Oleada 1 — 8 lanes disjuntas por fichero, god-files congelados

| lane | ficheros exclusivos | fichas | gate |
|---|---|---|---|
| **L1 · render de cable** | `LFPG_CableRenderer.c` | R01 R03 R05 R06 R07 R08 R09 R10 R11 R20 R21 R22 R24 (13) | linter delta + arranque; **no** aplicar las recetas `:3400` y `:2510` |
| **L2 · grafo** | `LFPG_ElecGraphImpl.c` | G08 G09 G10 G11 G14 G15 G19 G20 (8) | linter delta |
| **L3 · dispositivos A** | `LFPG_RemoteController.c`, `LFPG_Intercom.c`, `LFPG_Furnace.c` | D08 D09 D23 D11 D12 D14 D22 (7) | linter delta |
| **L4 · dispositivos B** | `LFPG_IDevice.c`, `lfpg_devicebase.c`, `LFPG_ActionDismantleDevice.c`, `LFPG_ActionUpgradeWaterPump.c`, `LFPG_DoorController.c`, `LFPG_MotionSensor.c`, `LFPG_SwitchV2Remote.c`, `LFPG_TestDevices.c`, `LFPG_BatteryAdapter.c` | D10 D21 D15 D06 D07 D13 D17 D18 D19 D24 (10) | linter delta; **D13 declara su contrato** controller↔planner |
| **L5 · ficheros y ajustes** | `LFPG_FileUtil.c`, `LFPG_Settings.c`, `LFPG_Migrators.c` | SEC10 SEC16 SEC19 SEC11 SEC18 (5) | linter delta; **precede a las lanes que consumen su parser** |
| **L6 · nucleo del sorter** | `LFPG_SorterData.c`, `LFPG_Sorter.c`, `LFPG_RPCClientHandler.c` | S01 S02 S16 S17 S09 (5) | linter + `ui_reconcile.py`; **S09 declara su contrato de ACK** |
| **L7 · UI del sorter** | `test/LFPG_SorterController_TEST.c`, `test/LFPG_SorterTagView_TEST.c` | S11 S20 S21 S23 (4) | `ui_reconcile.py` obligatorio |
| **L8 · render menor y HUD** | `LFPG_LaserBeamRenderer.c`, `LFPG_DeviceInspector.c`, `LFPG_Camera.c`, `LFPG_CameraViewport.c`, `LFPG_SearchlightController.c`, `LFPG_CableParticle.c`, `LFPG_CableHUD.c`, `LFPG_TankHUD.c` | R18 R19 R12 R13 R28 R14 R29 R17 R23 R27 R16 (11) | linter + `ui_reconcile.py` |

**63 fichas cerradas en la oleada 1, sin tocar un solo god-file.**

### Oleada 2 — los dos god-files, serializados

| lane | fichero | fichas |
|---|---|---|
| **L9 · manager** | `LFPG_NetworkManagerImpl.c` | G03 G05 G06 G07 G12 G13 G21 G22 G23 S05 S06 S24 SEC05 (13) |
| **L10 · handlers RPC** | `LFPG_RPCServerHandlerImpl.c` | S03 S12 S14 SEC04 SEC06 SEC07 SEC17 (7) |
| **L11 · sesiones y logica** | `LFPG_SorterLogic.c`, `LFPG_ControlSessionRegistry.c`, `LFPG_BTCSessionRegistry.c`, `LFPG_RPCGuard.c`, `LFPG_MissionInit.c` | S07 S15 S19 SEC08 SEC12 SEC13 SEC14 SEC15 R30 (9) |

L11 es disjunta y **puede adelantarse a la oleada 1** si hay revisor libre.

### El techo real

Por conflicto de fichero caben **8 a 13** lanes. Pero `fable` y `grok` llegan por separado a lo
mismo: **el techo lo pone la revision, no git.** `grok` lo cifra en **4 simultaneas para un
revisor**. Recomiendo **dos lotes de 4**, no ocho de golpe: cada lane exige dictamen de otra
familia y el arranque diag va por lote, no por lane.

---

## 6. LO QUE NO SE HACE

- **No se parte el dios.** 4 lanes contra 1 y el censo en contra.
- **No se saca `_TEST`.** Dejaria el sorter sin panel.
- **No se portan las 3 recetas de la auditoria** que cambian comportamiento (§3).
- **No entran los 7 «NUEVO-perfil»** hasta que haya una medida (pendiente de D5).
- **No se re-ejecuta `PLAN-CONJUNTO.md`.** Su T0b esta hecho en sustancia; las secciones bajas
  del HANDOFF que lo pintan pendiente son sedimentacion.
- **No se programan gates in-game de desmontar/desplegar/cablear** como si fueran automaticos:
  `action_use` no completa acciones de barra.

---

## 7. PROCEDENCIA

| lane | modelo servido (oraculo) | estado | evidencia |
|---|---|---|---|
| `fable` | subagente Anthropic | `COMPLETE` | 50.250 B, 7/7, 46 filas en B, 23 min, 37 tool_uses |
| `astra` | `gpt-6-astra` | `COMPLETE` | 61.066 B, 7/7, `turn.completed`, 24 min |
| `grok` | «Cursor Grok 4.6 Extra High» == pin | `COMPLETE` | 36.955 B, 7/7, `result:success`, 10 min, 188 tool_calls |
| `kimik3` | «Kimi K3 Max» == pin | `COMPLETE` | 45.136 B, 7/7, `result:success`, 12 min, 114 tool_calls |
| `glm` | «GLM 5.2 Max» == pin | `COMPLETE` | 40.989 B, 7/7, 3m45s, 80 tool_calls |
| `gemini` | **`gemini-3.5-flash` ≠ pin `3.7-flash`** | **`QUOTA`** | 2 intentos, `429` free tier. No entra al arbitraje |

⚠ **`arbitro=lane`:** el arbitro y `fable` son ambos Anthropic. Las filas de §3 sostenidas por
`fable` llevan esa marca; ninguna decision de §5 depende solo de `fable`.

⚠ **Concentracion por puerta:** 3 de las 5 lanes vivas entraron por Cursor. El acuerdo
`grok`+`kimik3`+`glm` es **mas debil de lo que parece** por compartir puerta, aunque no familia
de modelo. El acuerdo que si es fuerte es `grok`+`kimik3`+`astra`+`fable`: cuatro puertas, tres
familias.

**Deuda declarada:** el protocolo (`C2`) dice que un council no planifica un plan. Aqui aporto
tres cosas que una lectura mia no habria dado — la muerte de S2/§6.2/S6/H9, el reencuadre de
`_TEST`, y el aviso de las tres recetas peligrosas — asi que **esta corrida cuenta como
evidencia CONTRA C2 en su forma absoluta**, y a favor de matizarla: un council no planifica,
pero si adjudica un solape que nadie ha cruzado.

---

## 8. DECISIONES DEL DUEÑO — firmadas 2026-09-08

| # | decision | resultado |
|---|---|---|
| **D1** | Los 7 CONFLICTO del dinero | **Aceptados como riesgo.** Quedan apuntados como issues en el backlog con su cita; se revisan con calma mas adelante. **Prioridad minima.** No entran a ninguna lane de codigo. |
| **D2** | T0c (renombrar `_TEST` → canonico) | **CANCELADO.** Sale del backlog. El coste de conservar es una subclase vacia de 3 lineas; el de renombrar, romper bases de jugadores. |
| **D3** | `archive/t2-autoridad-servidor` | **ARCHIVADA.** No se fusiona. `main` ya tiene T2 por `l4` con dictamen verde; traer esas 1.162 lineas reintroduciria un paralelo historico. Lo que queda de T2 es in-game. |
| **D4** | PBO desplegado en `P:\Mods` | **Se queda el build nuevo** (`8d558ae0…`). Produccion sigue respaldada en `_staging\@LFPowerGrid_PRODUCCION_98ceb1eb.pbo`. Anotado para que nadie lo confunda con produccion. |
| **D5** | Los 7 «NUEVO-perfil» | **ENTRAN**, priorizados por facilidad. ⚠ **Correccion del dueño a la recomendacion del orquestador**, y tiene razon: recortar no es solo huella, es **reducir superficie de fallo**. La invariante «cortar lineas no reduce el arena» refuta el argumento de BYTES, no el de simplificacion. El orquestador confundio los dos objetivos. |

### D5 — reparto de los 7 por facilidad

| item | fichero | dificultad | lane |
|---|---|---|---|
| **C12** cuatro `DrawLine` por junta | `LFPG_CableHUD.c:349-352` | trivial | **L8** (ya existia) |
| **C11** laser sin cache entre frames | `LFPG_LaserBeamRenderer.c:157,188,264,299` | facil | **L8** |
| **C5** oclusion, 2 `GetScreenPos` | `LFPG_CableRenderer.c:2812` | facil | **L1** |
| **C2** tres pasadas O(n)/frame | `LFPG_CableRenderer.c:2710` | media | **L1** |
| **H2** scan por prefijo | `NM:3401,3476` | media | **L9** (oleada 2) |
| **H3** fallback O(V·W) | `NM:4733,4748` | media | **L9** |
| **U5** censo unico por turno | `NM:6830` | media | **L9** |
| **U6** tick de mantenimiento cliente | cruza varios ficheros | **compleja** | **PRIO MINIMA**, fuera de estas oleadas |

`H6` (celdas O(P·C), `NM:7027`, `m_PlayerCellsBuiltThisTurn` con 0 hits) entra tambien en **L9**.

**Ninguno de los cuatro faciles cuesta una lane nueva**: caen en L1 y L8, que ya existian.

### Lane extra que abre D5

| lane | ficheros | trabajo | riesgo |
|---|---|---|---|
| **L12 · higiene de assets** | solo `data/` y `config.cpp` (lecturas) | §5.2 junk (`data/kits/kitboxtexture.png` 2.340.485 B + su `.paa` 1.219.223 B, **cero refs**; `*.ogg.stereo_backup`; `Furnace_mono.ogg` huerfano) y §5.3 duplicados por sha256 (`switch_v1_co.paa` == `switch_v1_remote_co.paa`) | **cero**: no toca una linea de Enforce |
