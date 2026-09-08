<!-- Extraido del stream de cursor-agent: --mode ask no puede escribir en el workspace -->
<!-- modelo servido: Cursor Grok 4.6 Extra High | parada: success | coste: None | llamadas a herramienta: 150 -->
Voy a leer `BRIEF-REVIEW.md` y seguirlo completo. Primero cargo el brief y el contexto de revisión.El brief pide un dictamen adversarial: primero el encargo original, el diff y el informe del implementador.Siguiente: el diff real y el código modificado, para contrastar cada ficha con lo que hay en disco.Voy a contrastar cada cita del informe con el código y a leer los caminos de persistencia, calor, energía y desmontaje.Estoy en **Ask mode**: no puedo crear `DICTAMEN.md` en disco. El dictamen completo va aquí; en Agent mode se puede volcar al fichero sin rehacer la revisión.

VERDE. 0 GRAVE / 1 MEDIO / 2 MENOR.

---

# DICTAMEN.md

VERDE. 0 GRAVE / 1 MEDIO / 2 MENOR.

Revisión adversarial contra `BRIEF.md`, `CAMBIOS.diff`, `INFORME.md` y el árbol ya modificado. No se compiló Enforce ni se arrancó el juego.

### MEDIO — D03 deja ActionCondition de desmontaje partida entre cliente y servidor

**Dónde:** `scripts/4_World/LFPG_Battery.c:126`, `scripts/4_World/LFPG_Battery.c:351-359`, consumidor `scripts/4_World/LFPG_ActionDismantleDevice.c:116`.

**Qué está mal:** `LFPG_BlocksDismantle` ahora consulta `LFPG_GetStoredEnergy()`. En servidor ese getter devuelve el float autoritativo (`m_StoredEnergy`). En cliente, el `#else` sigue siendo el snapshot `m_StoredEnergyX10 / 10.0`. Una carga en `(0, 0.1)` es `> 0` en servidor y `0` en cliente: el menú ofrece desmontar y el `Can()` del servidor lo niega.

**Por qué importa:** el defecto de contabilidad del brief sí se cierra, pero el guard extra abre un desacuerdo cliente/servidor que antes no existía (ambos lados leían X10). El informe lo declara a propósito; sigue siendo frágil para el jugador.

**Qué habría que hacer:** que el guard de desmontaje use, en los dos lados, un criterio visible en ambos (el SyncVar X10, o un bool/int sincronizado “tiene energía”), y no un float que el cliente no ve.

### MENOR — Código nuevo en tabs dentro de ficheros indentados a espacios

**Dónde:** p. ej. `scripts/4_World/LFPG_Furnace.c:148-149`, `scripts/4_World/LFPG_Battery.c:77-78`, `scripts/4_World/lfpg_devicebase.c:594`.

**Qué está mal:** las líneas añadidas usan tabulador; el resto del método sigue en espacios. No es un diff inflado por CRLF ni un reordenado masivo.

**Por qué importa:** no rompe el arreglo; ensucia el diff y el grep visual. El brief pedía tabs en lo nuevo y no reformatear lo ajeno: se cumplió lo segundo a costa de un fichero mixto.

**Qué habría que hacer:** dejar constancia; no reindentar el fichero entero en esta lane.

### MENOR — Comentario de la acción de desmontaje sigue mintiendo

**Dónde:** `scripts/4_World/LFPG_ActionDismantleDevice.c:27`.

**Qué está mal:** la lista de excluidos aún dice que `LFPG_BatteryAdapter` se recoge directo. El código de la acción ya no lo excluye: `LFPG_GetKitClassname()` en `scripts/4_World/LFPG_BatteryAdapter.c:85-87` devuelve kit.

**Por qué importa:** no bloquea la recuperación. Quien lea solo el encabezado de la acción creerá el contrato viejo. El fichero no está en la lista blanca; el informe lo declara.

**Qué habría que hacer:** corregir ese comentario en una lane que pueda tocar el fichero.

---

## Tabla ficha por ficha

| Ficha | Brief | Implementador | Revisor |
|---|---|---|---|
| D01 Horno reinicia el plazo al apagar/encender | P1 confirmada | ARREGLADA | **ARREGLADA** |
| D02 Calor restaurado antes de crear UTS | P1 confirmada | ARREGLADA | **ARREGLADA** |
| D03 Cuantización X10 en contabilidad | P1 confirmada | ARREGLADA | **ARREGLADA** (con el MEDIO de desmontaje) |
| D04 GhostPAS sin CfgVehicles | P1, no implementar | PENDIENTE-DECISION | **PENDIENTE-DECISION** (correcto: cero diff en Intercom/config) |
| D05 BatteryAdapter irrecuperable | P1 confirmada | ARREGLADA | **ARREGLADA** |

### D01 — comprobación

El abuso era reescribir `m_BurnNextMs = now + 30000` en cada encendido. Eso ya no ocurre: al apagar se guarda el resto (`LFPG_ToggleFurnace` en `scripts/4_World/LFPG_Furnace.c:559`, igual en muerte `:305` y corte de cables `:333`); al encender se reconstruye el deadline con ese resto (`:597`) y se llama `LFPG_BurnTick()` en el acto (`:599`) para cobrar un intervalo ya vencido. El mismo patrón está en la restauración (`:267-269`).

No persisten el instante absoluto de misión; persisten la duración restante y suben `LFPG_GetDevicePersistVersion` a 3 (`:429-432`). Encaja con `GetTime()` de misión, no con un timestamp durable. v1/v2 cargan sin el campo y arrancan un intervalo completo (`:466`, `:477-489`). El lector base acepta `deviceVer` hasta 999 (`scripts/4_World/lfpg_devicebase.c:72`); el informe admite que un rollback de código deja un int sin leer. Eso no reabre el exploit de toggle.

### D02 — comprobación

`LFPG_OnInitDevice` corre dentro de `super.EEInit()` (`scripts/4_World/lfpg_devicebase.c:267` → `scripts/4_World/LFPG_WireOwnerBase.c:77`) **antes** de `m_UTSource = new ...` en `scripts/4_World/LFPG_Furnace.c:147`. El `LFPG_SetHeatActive(true)` que vivía al final de `OnInitDevice` se ha quitado; tras crear la fuente se llama `LFPG_SetHeatActive(m_SourceOn)` (`:149`). El helper sigue siendo no-op si no hay UTS (`:155-162`). El camino de error silencioso del brief queda cerrado. `FurnaceHeatEnabled==false` sigue sin crear UTS: es ajuste, no el bug.

### D03 — comprobación

`m_StoredEnergy` es el float de servidor (`scripts/4_World/LFPG_Battery.c:78`). `LFPG_GetStoredEnergy` lo devuelve bajo `#ifdef SERVER` (`:351-354`); `LFPG_SetStoredEnergy` lo escribe antes del snapshot X10 (`:365-367`). Persistencia escribe el float de siempre (`:301`); la carga ya no pasa por `int x10` para el valor autoritativo (`:339-341`). El gestor no hace falta: `scripts/5_Mission/LFPG_NetworkManagerImpl.c:7539` y `:7722` ya van al getter/setter (el brief citaba ~7644; esa línea es el delta de descarga). El adaptador sigue leyendo CompEM (`scripts/4_World/LFPG_BatteryAdapter.c:475-486`). La asimetría de precisión del brief en el camino BatteryBase queda cerrada. No tocaron `LFPG_NetworkManagerImpl.c` (zona ~7600-7750 reservada): acertado.

### D04 — comprobación

Sin cambios en `LFPG_Intercom.c` ni `config.cpp`. Las dos opciones del informe coinciden con el árbol:

**(a)** El hermano está en `config.cpp:2562-2572`; CfgVehicles cierra en `:2573`. El padre de script es `Land_Radio_PanelPAS` (`scripts/4_World/LFPG_GhostPASBroadcaster.c:24`). Ya hay `class PASReceiver;` en `config.cpp:239`; no hay forward de `Land_Radio_PanelPAS`. El bloque propuesto es copiable. Declararlo no prueba voz PAS.

**(b)** Miembro `m_GhostPAS` en `scripts/4_World/LFPG_Intercom.c:118-119`. Bloque spawn/destroy/update `:812-887`. Cinco `LFPG_UpdateGhostPAS()` en `:266`, `:372`, `:565`, `:900`, `:923`. Dos `LFPG_DestroyGhostPAS()` en `:320` y `:346`. Fichero `LFPG_GhostPASBroadcaster.c` líneas 1-110. Cabecera PAS `:9` y `:11-17`. No incluye RF (`m_BroadcastEnabled`, GhostRadio). La lista es usable.

### D05 — comprobación

El kit existía: script `scripts/4_World/LFPG_BatteryAdapter.c:41-45`, config `config.cpp:2100` (`scope = 2`). El agujero era `LFPG_GetKitClassname` devolviendo `""`. Ahora devuelve `"LFPG_BatteryAdapter_Kit"` (`:85-87`) y el comentario deja de prometer la tecla F (`:84`). `lfpg_devicebase.c:594` ya no cita el adaptador como no desmontable. Los tres guards heredados (`CanPutInCargo` / `CanPutIntoHands` / `CanBePlaced` más quitar Take en `:430-455`) se conservan a propósito: el dispositivo desplegado sigue sin caber en inventario (`itemSize[] = {0,0}`, `isDeployable = 0` en `config.cpp:2127` y `:2131`). La vía es destornillador: registro `scripts/4_World/LFPG_ActionRegistration.c:116` y `:157`; la acción mira kit no vacío (`:94`), attachments (`:99-100`), cargo (`:104-108`) y `LFPG_BlocksDismantle` (`:116`). El adaptador no overridea ese último: con batería acoplada `AttachmentCount() > 0` bloquea y se conserva la carga en el vanilla. Decisión correcta frente a “cumplir F”.

## Alcance, Enforce, informe

Lista blanca: el diff toca solo `LFPG_Furnace.c`, `LFPG_Battery.c`, `LFPG_BatteryAdapter.c`, `lfpg_devicebase.c`. No hay escritura en `LFPG_NetworkManagerImpl.c`, `LFPG_Intercom.c` ni `config.cpp`. `INFORME.md` es el entregable, no un choque con otras lanes.

Líneas añadidas: sin `? :`, `++`/`--`, `+=`/`-=`, `foreach`, `Print(` ni `ref` en params/locales. `LFPG_Util.Error/Info` donde hay log nuevo.

Citas del informe: las de comportamiento casan al abrirlas (a veces apuntan al inicio de la función, no al `return` interior). No hay cita inventada que invalide el dictamen. `LO QUE NO PUDE VERIFICAR` del informe es de verdad: compile, persistencia binaria, in-game, PAS, desmontaje real. No relleno.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

D01 no se arregla persistiendo `m_BurnNextMs`. Es tiempo de misión (`GetTime()`), no marca de pared: al reiniciar el absoluto miente. Hacía falta una semántica que el brief no fijó. Pausar al apagar es la lectura conservadora de “quema mientras está encendido”; no es la única. Otra, no implementada, sería cobrar tiempo de apagado o un tick entero por encendido. Con la pausa un jugador puede retrasar el consumo dejando el horno apagado; eso ya no es el exploit de reiniciar los 30 s.

El bump a deviceVer 3 es consecuencia de guardar el resto, no un capricho. El lector viejo acepta hasta 999 y solo lee dos campos de horno: un downgrade de código con datos v3 deja un int en el stream. Eso no lo inventó esta lane; lo arrastra `LFPG_DEVICE_PERSIST_VER_MAX_ACCEPTED = 999` en `lfpg_devicebase.c:72`. Pedir persistir el plazo y a la vez rollback limpio era incompatible sin tocar ese techo (fuera de la ficha, y `lfpg_devicebase.c` solo se tocó el comentario de D05).

D03 no exigía editar el gestor. La cuantización estaba en el getter de BatteryBase; las llamadas reales están en `:7539`/`:7722`, y `:7539` queda fuera del tramo ~7600-7750 que el brief autorizaba. Obligar a tocar esa zona habría sido un choque de lanes para un no-cambio. Quitar X10 del round-trip no iguala BatteryBase con el adaptador: el adaptador vive en CompEM × 2.0, otra escala.

D04: falta la clase en CfgVehicles y `CreateObjectEx("LFPG_GhostPASBroadcaster")` no puede materializarla. Declarar el bloque (a) no demuestra alimentación, `SwitchOn` nativo ni voz hacia Speakers. Borrar “todo broadcast” rompería RF: `m_BroadcastEnabled` alimenta también GhostRadio. Las dos lecturas del brief son reales; dejarlas al dueño era la única opción legal.

D05 describe mal “sin kit”. El kit está en script y en config público. Lo que no había era camino hasta él. Cumplir la promesa de recoger con F habría chocado con el contrato de dispositivo desplegado (guards de `LFPG_DeviceBase`, `itemSize` 0, `isDeployable=0`) y habría dejado un aparato cableado metible en la mochila. Corregir el comentario y abrir desmontaje por kit es la cura que el código ya tenía a medio hacer.

El comentario de `LFPG_ActionDismantleDevice.c:27` queda mentiroso a propósito de la lista blanca, no por olvido oculto.

## LO QUE NO PUDE COMPROBAR

- Compilación Enforce al cargar mundo (cliente y servidor por separado). Nadie puede invocar el compilador aquí.
- Comportamiento in-game: toggle del horno vs sondeo ~5 s, calor UTS tras restart, ticks de batería, desmontaje con destornillador, dos jugadores a la vez.
- Stream binario real de `OnStoreSave`/`OnStoreLoad` (v2→v3, truncado, lector 8de29d5 contra bytes v3).
- Las simulaciones JS y el linter que el informe dice haber corrido: no hay artefactos de esas corridas en el workspace.
- Si `#ifdef SERVER` en el getter deja muerto `m_StoredEnergy` en listen/offline. El resto del mod ya vive detrás de ese define; no es un fallo nuevo demostrable solo con lectura.
- Voz PAS, `CreateObjectEx` de la clase fantasma, ni el ciclo RF si se eligiera (a) o (b).
- BOM/CRLF byte a byte; el diff no parece una normalización masiva de finales de línea.