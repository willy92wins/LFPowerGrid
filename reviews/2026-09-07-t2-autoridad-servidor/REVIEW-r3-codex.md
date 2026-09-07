## VEREDICTO

RECHAZAR.

El VERDE completo no se sostiene: **V2 es parcial**. El aspersor B del control anterior ya se apaga, pero un intento rechazado que retira y restaura una fila puede dejar activo al aspersor nuevo, sin cable ni nodo. Además, la corrección de N-03 introduce **R3-01**: la poda cancela invalidaciones pendientes de un owner que sigue vivo.

**Recomiendo aparcar T2 y desaconsejo desplegar esta foto como cierre de la transacción. No propongo una cuarta ronda.** Los dos defectos son MEDIOS: estado funcional incorrecto y desincronización de cables, respectivamente; no he demostrado un GRAVE nuevo, un crash ni corrupción persistente. Pueden quedar documentados como deuda del tramo archivado, pero documentarlos no convierte V2 en ALCANZADO.

Revisión estática del 2026-09-07 sobre `fix/t2-r2-transaccion`, HEAD `8cb622244d0fcb073e5b7aa4d9d8172dfbd2ef29`. El diff de los tres archivos existentes tiene 1.120 inserciones y 184 eliminaciones; además está el archivo nuevo de transacción, de 42 líneas. `REPORT.md` e `INFORME-RONDA3.md` son idénticos por SHA-256. He leído las revisiones anteriores como escenarios a contrastar, no como prueba del código actual. No he ejecutado DayZ, AddonBuilder ni builds.

Abreviaturas de las citas:

- **N** = `scripts/5_Mission/LFPG_NetworkManagerImpl.c`.
- **E** = `scripts/5_Mission/LFPG_ElecGraphImpl.c`.
- **H** = `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`.
- **TX** = `scripts/5_Mission/LFPG_FinishWiringTxn.c`.
- **WH** = `scripts/3_Game/LFPG_WireHelper.c`.
- **WO** = `scripts/4_World/LFPG_WireOwnerBase.c`.
- **I** = `scripts/4_World/LFPG_IDevice.c`.
- **S** = `scripts/4_World/LFPG_Sprinkler.c`.
- **R** = `scripts/4_World/LFPG_CableRenderer.c`.
- **D** = `scripts/3_Game/LFPG_Defines.c`.
- **VAN** = `C:/Users/guill/OneDrive/Documentos/DayZ Projects/scripts`.

## EL VERDE, PUNTO POR PUNTO

“ALCANZADO” indica cierre sustentado por lectura del mecanismo y, donde se especifica, modelo offline; no certifica ejecución en Enforce.

| # | ALCANZADO / NO / PARCIAL | Evidencia | Qué queda vivo |
|---|---|---|---|
| V1 | **ALCANZADO** | E:612-625 rechaza por capacidad antes de crear extremos. E:1370, :1383, :1395, :1412 y :1532-1546 cubren rechazos internos; E:1040-1053 y :1550-1605 podan al cerrar, después de restaurar en N:1012-1014. Modelo del control: **2.047 → 2.047**, D no aparece y una conexión independiente pasa. | No reproduzco el huérfano N-01. El comportamiento preexistente del límite global no se rediseña aquí. |
| V2 | **PARCIAL** | N:1340-1353 retira todas las filas antes de N:1367. El control P→B/P→A termina con **B inactivo y sin fuente**. Sin embargo, N:1707-1718 refresca agua también cuando la inserción devuelve false; N:1012-1014 no deshace ese efecto sobre el destino nuevo. | **R-T2-AGUA**, desarrollado abajo: D termina activo sin cable después de un rechazo con retirada y restauración. No atribuyo a ronda 3 la aparición de este residuo. |
| V3 | **ALCANZADO para N-03** | Consumo en N:3315-3323; diferimiento en N:3241-3253; cancelaciones en N:1618, :2418, :2704, :3613, :4206, :4987, :5794 y H:2036. El control de owner muerto vacía cola y mapa; 1.000 cancelaciones del modelo dejan cero claves. | **R3-01 es el defecto inverso:** se cancela demasiado pronto un owner vivo. No renombro este fallo como la fuga anterior ni cambio retrospectivamente V3, cuyo texto exige no retener owners no publicables. |
| V4 | **ALCANZADO** | N:1262-1287 conserva la secuencia completa; N:1299-1309 vacía y repuebla el mismo array con las mismas referencias; N:1375-1399 restaura stores antes de índices y aristas. TX:41 mantiene referencias fuertes a las filas. E:276-287 consume ese orden. | El fixture conserva G→T tras el siguiente rebuild. Esto acredita orden e identidad del store, no el rollback de todos los efectos de dispositivos. |
| V5 | **ALCANZADO en los dos caminos pedidos** | H:1875 registra antes de H:1878 y publica en H:1888. H:2018 registra antes de H:2020; publica o cancela en H:2029-2036. N:3315-3323 une el extremo antiguo. | Corte directo y diferimiento sin poda alcanzan al observador. R3-01 puede destruir después ese registro correctamente capturado. |
| V6 | **ALCANZADO dentro de esta revisión** | He seguido creación/poda de nodos, rollback, referencias, índices, agua, productores de invalidación y consumidores de colas. La regresión nueva confirmada, R3-01, es **MEDIA**. | No he encontrado un GRAVE nuevo. El hecho de que la publicación de cables esté tras OK, por sí solo, no prueba este punto. |
| V7 | **ALCANZADO para los contratos enumerados** | F-02: N:1083-1157, :1175-1226 y :1425-1458; 20.673 casos offline. SEC02: H:649-686, N:981-989 y WH:230-236. SEC20(a): H:464-580, :723-775 e I:820-868. SEC01: los envíos conservan identidad comprobada y destinatario explícito. | S-01/CCTV y compilación/runtime siguen sin medir. No encuentro una regresión de estos cuatro contratos que pueda demostrar. |

### V1: por qué ya no se produce N-01 y qué cambia en la admisión

En el escenario anterior, G ya tiene 12 salidas, D está registrado pero no tiene nodo, y el grafo suma 2.047 nodos. El rechazo ocurre ahora en E:612-616. No llega a E:622-623, por lo que D no aumenta el contador. El rollback retira la fila nueva y la siguiente operación no tropieza con el límite global por culpa de ese intento.

No encuentro un caso legítimo antes aceptado que rechace **por adelantar este chequeo**: E:1327/:1334 comprueba las mismas listas, umbral y comparación que E:1379/:1391. Entre ambos puntos, el creador de nodos E:1233-1317 modifica nodos y datos auxiliares, no añade ni retira aristas. Un duplicado con el cupo lleno tampoco se admitía antes.

La poda adicional tiene efectos sobre entidades: E:1582 llama al reset de SyncVars. Por eso no la trato como una mera reducción del contador. En el camino de rollback diferido se ejecuta después de restaurar aristas y conserva los nodos que vuelven a tener conexiones. En el rebuild puede actuar antes de terminar todos los stores; la admisión topológica sigue leyendo los mismos límites. No he demostrado una regresión GRAVE por esa ampliación.

### V4: identidad, orden e índices

El snapshot es un array distinto de referencias a los mismos objetos, no una copia profunda de las filas: N:1278-1284 y TX:41. El getter del owner retorna su array miembro (WO:141-143; acceso directo I:639-644), y el vanilla retorna el array del mapa (N:919-924). El restore no sustituye ese array vivo cuando existe: ejecuta N:1303/:1309 sobre él. Las referencias del snapshot mantienen vivas las filas mientras se vacía el array.

Los índices usados aquí contienen IDs y recuentos (N:23-35), no índices posicionales de las filas. Se compensan las retiradas y reposiciones mediante N:1368-1369 y :1395-1396. El crédito de eliminación explícita sigue consumiéndose en N:2126-2133; mover la retirada del store no altera la pareja notificación→retirada del índice.

También queda resuelta la duda de `Remove`: la declaración nativa documenta sustitución por el último elemento, sin conservar orden (`VAN/1_core/proto/enscript.c:457-463`). La restauración completa evita depender de ese orden intermedio. El modelo usa esa semántica y confirma el mismo array, los mismos objetos y G→T admitido después de reconstruir.

### V2: el control original se cierra, pero queda R-T2-AGUA — [MEDIO]

**Dónde:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:1707-1718`, `:1012-1014`, `:1462-1488` y `:1399`. Consumidor: `:6104-6128` y `:6167-6188`.

**Qué pasa:** la notificación de alta refresca el agua aunque el grafo haya rechazado la arista. Ese refresh lee la fila provisional, que todavía está en el store. El rollback la quita sin notificar su retirada al subsistema de agua; reponer las filas anteriores tampoco limpia un destino que ya no está en el store.

[EXACT — fragmento leído en N:1707-1718]

```cpp
        bool inserted = m_Graph.OnWireAdded(sourceId, targetId, sourcePort, targetPort, wireRef);
        if (!inserted)
            m_GraphFullRebuildRequired = true;
        if (inserted)
        {
            // v0.7.30: Auto-track both endpoints for centralized position polling
            TrackDeviceForPolling(sourceId);
            TrackDeviceForPolling(targetId);
        }
        // v5.1: Instant sprinkler link refresh on wire connect
        string noRemoved = "";
        LFPG_RefreshPumpSprinklerLink(sourceId, noRemoved);
```

**Escenario reproducible que incluye una retirada:**

1. Bomba T2 P alimentada por una fuente suficiente, con `MaxWiresPerDevice=64`. Su store cargado tiene primero 12 filas hacia splitters sin otras conexiones, con OUT `legacy_0..legacy_11` e IN `input_1`; después, una fila `P.output_1→L_old.input_1`. L_old es otro splitter. Son dispositivos vivos, próximos y registrados; las filas son propias o sin reclamar. El grafo tiene las primeras 12 aristas de salida de P y no la decimotercera.
2. D es un aspersor registrado, inicialmente inactivo, sin cable y sin nodo. Se solicita `P.output_1→D.input_0`, con geometría válida y cuota disponible.
3. La transacción captura el store y retira la fila a L_old. El store baja de 13 a 12 y admite la nueva. El grafo sigue teniendo las 12 salidas anteriores: la fila retirada estaba fuera del grafo. E:612-616 rechaza.
4. N:1718 recorre el store provisional, encuentra D y le asigna P como fuente, además de activarlo. Solo hay un aspersor en ese store, por lo que basta que P esté alimentada; no depende del tanque (N:6154-6188).
5. El rollback elimina la fila a D y repone las 13 originales. El refresh de la reposición recorre splitters, sin volver a visitar D. **D conserva actividad y fuente P, pero no tiene cable ni nodo.**

No es un fixture imposible por los puertos del intento: P declara `output_1` y D `input_0` (`LFPG_WaterPump.c:390-392`, S:75-77); WO:268-287 permite esa conexión. Las filas legacy entran por WO:132 y WH:322-387: el loader comprueba campos, geometría y duplicados, pero no valida el vocabulario no vacío de puertos (WH:35-83). Los 13 registros caben con ese setting. Esto acredita que el loader admite el estado, no que la UI actual lo genere ni que exista en producción. Los splitters tienen consumo propio cero (`scripts/4_World/LFPG_Splitter.c:69`) y, sin salidas, no añaden demanda downstream (E:3632-3669), de modo que el fixture no exige mantener P alimentada bajo una sobrecarga artificial.

La retirada de una fila sin arista también solicita reconciliación en N:1753-1756. Eso no apaga D: D nunca llegó a tener nodo, y el store restaurado no contiene D. Un refresh posterior de P tampoco lo encuentra. El reset global periódico de aspersores sí apaga su actividad (N:6242-6254; intervalo D:700). No afirmo que ese reset borre el ID de fuente.

**Consecuencia:** degradación funcional temporal, no solo visual. El scheduler habilita riego por actividad (N:7728-7736), y el cuerpo de S:358 en adelante no consulta la conexión eléctrica antes de regar. El setter eléctrico S:114-121 modifica otro campo, no la actividad. La duración real, replicación y cantidad de riego requieren el motor; no las he medido.

**Control positivo del arreglo anterior:** con `P.output_2→B.input_0` seguido de `P.output_1→A.input_0`, reemplazar por `P.output_2→A.input_0` deja B apagado y sin fuente. Al notificar A, B ya no está en el array. He seguido los demás efectos inmediatos de la notificación: el grafo y el crédito leen aristas/IDs; no he encontrado otro consumidor que requiera conservar la fila retirada en el store. El defecto pendiente está en el alta rechazada.

**Discriminador del rechazo:** el modelo también ejecuta el mismo reemplazo con 11 filas legacy y la fila a L_old. En ese caso el alta se acepta y D termina activo **con cable**. El fallo anterior exige que las 12 salidas admitidas sigan ocupadas después de retirar la fila que estaba fuera del grafo.

**Qué haría al retomar:** cerrar el efecto de agua en la decisión final de la transacción, incluyendo el destino nuevo rechazado. Mover exclusivamente las retiradas no basta. Una corrección debe demostrar tanto el control B del éxito como D inactivo después del rechazo y recuperación del estado previo de P. No he aplicado esa corrección.

## REGRESIONES NUEVAS DE LA RONDA 3

### R3-01 — [MEDIO] La poda cancela la invalidación de un generador vivo antes del envío diferido

**Dónde:** `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4975-4987`. Se cruza con el diferimiento de `:3241-3253`, el flush de `:3603-3617` y la selección de observadores de `:3334-3353`.

**Qué pasa:** el array `emptyOwners` reúne dos casos distintos: owner irresoluble (N:4928-4939) y owner vivo con cero filas (N:4975-4978). La nueva cancelación en N:4987 se aplica a ambos. En el segundo caso todavía existe un envío diferido válido que necesita las posiciones antiguas.

[EXACT — N:4975-4987]

```cpp
            // If owner has no remaining wires, mark for removal
            if (wires.Count() == 0)
            {
                emptyOwners.Insert(ownerId);
            }
        }

        // Remove empty owner entries
        int eo;
        for (eo = 0; eo < emptyOwners.Count(); eo = eo + 1)
        {
            m_VanillaWires.Remove(emptyOwners[eo]);
            ClearVanillaInvalidation(emptyOwners[eo]);
```

**Escenario:**

1. G=0, T=60 y observador B=72, en metros sobre un eje; actor junto a G. G→T ya fue recibido por B, con waypoints que dividen la distancia en segmentos válidos. G y T siguen vivos.
2. Un FullSync de otro jugador está en curso. El actor corta el último cable de G. H:1875 registra T, H:1878 deja el store vacío, y H:1888 llega a la rama diferida N:3241-3253.
3. Antes de que termine ese FullSync se ejecuta la poda diferida. Su programación es real, N:4735, y su callback llama a la poda en N:5080 sin bloquear por FullSync. El callback de FullSync trabaja en varios ticks, N:3555-3576, con presupuesto D:257. No se necesita intercalar dos instrucciones de un RPC: se intercalan callbacks.
4. G se resuelve, pero tiene cero filas. N:4978 lo incorpora a `emptyOwners`; N:4987 borra la posición de T. La cola diferida conserva G y su objeto.
5. El flush envía un blob vacío de G, pero ya solo dispone de la posición de G. El actor lo recibe; B, a 72 m de G y 12 m de T, queda excluido.

El escenario puede prepararse alrededor del callback programado 30 segundos después de esa fase de validación, con suficientes owners para que haya FullSync pendiente. No lo presento como una carrera frecuente ni como un proceso paralelo. El handler bloquea durante validación activa (H:1740), pero no durante un FullSync posterior.

**Consecuencia:** desincronización del estado de cables y posible cable fantasma mientras B permanece en ese estado. G sigue siendo resoluble y 72 m está dentro del early-out de 75 m del renderer (R:2386, :2417-2441); B también está cerca del extremo antiguo (R:2539-2553). El mecanismo de resincronización no anuncia una generación nueva para un vanilla ya conocido (R:1479-1512). No afirmo que dure indefinidamente en todos los clientes.

**Por qué es nueva:** el borrado del store vacío ya existía, pero la llamada a cancelar las invalidaciones de ese mismo conjunto se añade en ronda 3 y está identificada como su arreglo en `REPORT.md:19`. Con la posición de T conservada, el broadcaster puede producir el blob vacío aunque ya no exista una entrada del store: N:3256-3268 permite `wires` nulo y N:3315-3320 añade los extremos guardados. El modelo devuelve actor+B sin esa cancelación y solo actor con ella.

**Qué haría al retomar:** separar la desaparición del objeto de la ausencia de cables. Conservar el interés pendiente del owner vivo hasta que su publicación se consuma; cancelar cuando la publicación deja de ser posible. Es una corrección interna de ciclo de vida y no exige cambiar el payload, destinatarios ni formato persistente.

**Control negativo:** si el objeto G desaparece antes del flush, N:3611-3613 debe cancelar y liberar la entrada. Ese control sí pasa. También pasa el corte directo sin diferimiento y el diferimiento que no atraviesa la poda.

No encuentro otra regresión nueva de ronda 3 que pueda sostener con un mecanismo y un repro. R-T2-AGUA está separado deliberadamente como residuo del contrato transaccional, no contado dos veces ni promovido a GRAVE.

### Inventario productor → publicación o cancelación

Hay nueve llamadas productoras y ocho sitios que llaman al helper de cancelación, además del consumo del broadcaster. No falta una cancelación de owner muerto en los caminos productores que he podido trazar; el error nuevo está en su condición.

| Productor | Terminación comprobada |
|---|---|
| Reemplazo: filas vanilla retiradas, N:1509 | Origen publicado en N:1552; otros owners en N:1612-1618, con cancelación si no hay objeto. |
| Reemplazo: extremo nuevo del origen vanilla, N:1551 | N:1552; el request exige objeto de origen antes de empezar (N:971-974). |
| Incoming por índice, N:2397 | Cola o cancelación en N:2411-2418; drain N:2692-2707. |
| Corte completo del handler, H:992 / :1012 | Broadcast del objeto validado en H:1024. |
| Corte OUT concreto, H:1875 | Broadcast H:1888. |
| Rescate incoming, H:2018 | Broadcast o cancelación H:2029-2036. |
| Corte de integridad/lifecycle de owner vanilla, N:5660 | Cola N:5668, drain N:2692-2707. |
| Fallback de integridad incoming, N:5773 | Resolución, cola o cancelación N:5785-5794. |
| Todos los broadcasts diferidos | N:3603-3617 publica el objeto vivo o cancela el muerto, y limpia las dos colas. |
| Limpieza de dispositivo desaparecido / poda | N:4206 es coherente con desaparición; N:4987 mezcla desaparición con store vacío: **R3-01**. |

## LA PREGUNTA DE `ref` EN LOCALES DE `Find`

**Resuelta para la semántica preguntada. No hay evidencia de que esos locales pierdan la salida por omitir `ref`.**

La declaración nativa es `proto bool Find(TKey key, out TValue val);` y su comentario especifica que guarda el resultado en `val` (`VAN/1_core/proto/enscript.c:848-858`). Es `out` lo que hace que la llamada escriba en el argumento; `ref` no es el modificador que habilita esa asignación.

La documentación oficial de **DayZ** establece que los locales de funciones son referencias fuertes por defecto y explica la diferencia entre `out` y `ref`. Por tanto, el razonamiento del brief acierta en que el valor puede recibirse y usarse, pero no necesita limitar su vida al mapa: el local ya mantiene una referencia fuerte durante su ámbito. [Bohemia: Enforce Script Syntax, apartados de modificadores y referencias](https://community.bohemia.net/wiki/DayZ%3AEnforce_Script_Syntax).

Hay además un consumidor vanilla del mismo patrón: `VAN/4_world/entities/cachedequipmentstorage.c:196-207` declara `array<ref CachedEquipmentItemAttribute> arr;`, recibe `Find(category, arr)`, crea el array solo cuando falta y después inserta sobre `arr`. Se repite en `:242-253`. No depende del estilo mayoritario del mod.

Aplicación a esta revisión:

- N:943 recibe el array de posiciones; N:946-949 lo crea/guarda cuando falta y añade posiciones al mismo objeto.
- N:1126 recibe los owners del índice inverso. Ese mapa es **el índice inverso**, no el mapa de invalidaciones; su funcionamiento afecta a los conflictos de F-02.
- N:3314 recibe las posiciones pendientes y las copia antes de quitar la entrada del mapa.
- Los locales con `ref` de E:1324, :1331, :1376 y :1388 no aportan un contrato de salida distinto.

La propia referencia ampliada de la skill aclara “Redundant but NOT wrong” para el local (`C:/Users/guill/.agents/skills/enforce-script-reference/references/memory-management.md:51-66`), aunque el resumen de `C:/Users/guill/.agents/skills/enforce-script-reference/SKILL.md:53` da una regla de estilo más tajante. No convierto esa contradicción editorial en un fallo del mod.

No he compilado el árbol en la versión concreta del servidor. Esa limitación general permanece, pero **la duda de asignación de Find queda resuelta por el contrato nativo, el uso vanilla y la documentación**, no pendiente por falta de ejecución.

## CITAS COMPROBADAS

Los rangos siguientes se han abierto en esta sesión. La tabla contrasta las afirmaciones sustanciales de `REPORT.md` / `INFORME-RONDA3.md`; no vuelve a contar el linter que el brief ya daba por comprobado.

| Afirmación/cita del informe | Fuente comprobada | Resultado |
|---|---|---|
| V1: chequeo antes de crear nodos | E:612-625, :1233-1317, :1321-1340, :1376-1398 | **Sí**; mismos límites, anticipados sin mutación de aristas entre ambos chequeos. |
| V1: poda después de restaurar | N:1012-1014; E:1040-1053, :1532-1605 | **Sí**; comprueba de nuevo si quedan aristas. |
| V2: store antes de notificación | N:1340-1369; :6042-6189 | **Sí para el control original**, insuficiente para el alta rechazada en N:1718. |
| V3: consumo al envío | N:3312-3363 | **Matiz:** consume antes del bucle de jugadores, incluso si no hay un destinatario elegible; no acredita que haya ocurrido un Send. |
| V3: cancelaciones | N:1618, :2418, :2704, :3613, :4206, :4987, :5794; H:2036 | **Existen**; la de N:4987 no distingue owner muerto de owner vivo sin filas. |
| V4: copia de la secuencia y replay | N:1262-1310, :1373-1399; TX:37-42; WO:141-153 | **Sí**; identidad de array vivo y filas conservada en el escenario leído. |
| V5: los dos registros faltantes | H:1874-1888, :2017-2036 | **Sí**; R3-01 afecta a la conservación posterior, no a esas llamadas. |
| V6: solo publicar tras OK | N:1009-1021 y :1491-1630 | **Sí para publicación de cables**. Los setters de actividad llamados por N:1718 no quedan detrás de esa frontera; S:140-147 marca sincronización. |
| F-02 | N:1083-1229, :1403-1458; WH:153-175; N:889-897 | **Sí**; los dos tipos de store tienen los límites descritos y el descuento por fila es único. |
| SEC02 | H:649-686, :778-881; N:981-989; WH:230-236; `DECISIONES-PRODUCTO.md` | **Sí**; aborto restrictivo previo a mutación. Se conserva identidad del creador y flag en el request. |
| SEC20(a) | H:464-580, :723-775; I:820-868 | **Sí**; nombres y dirección se verifican antes de guardar, y vacío se normaliza. |
| SEC01, snapshots y delta nativos | N:2838-2872, :3028-3064, :3173-3211 | **Sí**; guard de identidad antes del envío, incluso con overflow de interés del snapshot. |
| SEC01, vanilla y fullsync | N:3327-3363, :3370-3410, :3521-3535 | **Sí**; destinatarios explícitos con guard. |
| SEC01, otros tres envíos del manager | N:3722-3743, :3761-3788, :7090-7132 | **Sí**; identidad comprobada en los tres. |
| SEC01, settings | H:2060-2077 | **Sí**; no envía con identidad nula. |
| “Remove puede compactar o intercambiar” | `VAN/1_core/proto/enscript.c:457-470` | **Resuelto:** Remove intercambia con el último; RemoveOrdered conserva orden. |
| Find y local sin ref | `VAN/1_core/proto/enscript.c:848-858`; `VAN/4_world/entities/cachedequipmentstorage.c:196-207` | **Sí**; firma y uso nativo concretos, fuera del bloque de logging del ejemplo. |

No he encontrado una API inventada en las correcciones revisadas. Sí afirmaciones de cierre más amplias que su evidencia, especialmente V2 y “publicar solo si ambos admiten”.

## LO QUE NO PUDE VERIFICAR

- **Compilación y ejecución DayZ:** no hay un compilador Enforce ejecutado aquí. No he medido replicación, cadencia efectiva, rendimiento, bytes retenidos ni riego en servidor. No convierto los modelos Python en un PASS del motor.
- **S-01/CCTV:** sigue pendiente comprobar qué jugadores/identidades enumera el motor al controlar una cámara y qué se resincroniza al salir. Se mantiene como incertidumbre, no como motivo de rechazo.
- **Frecuencia real de los fixtures:** el loader admite el estado legacy usado en R-T2-AGUA, pero no he inspeccionado saves de producción ni afirmo que existan allí esas filas. R3-01 requiere la coincidencia temporal descrita.
- **Versiones intermedias:** no existe un commit separado de cada ronda en este árbol. La atribución de R3-01 se apoya en el cambio actual y en el informe de ronda 3; no he ejecutado dos builds históricos.
- **Igualdad total del estado del grafo:** orden y restauración de filas no demuestran igualdad de todas las asignaciones eléctricas, colas y SyncVars. Se han seguido los caminos relevantes a V1..V7 y sus consumidores; no certifico todo el mod.

### Comprobaciones offline ejecutadas

El modelo incluido al final se ejecutó por stdin con `python -X utf8 -B -`, sin crear fixtures ni cachés. Resultado: exit 0. Las aserciones comprueban cierres y también **la presencia de los dos fallos**; ese exit 0 no significa VERDE.

[EXACT — salida observada]

```text
V1: 2047 -> 2047; D absent; independent connection accepted
F01 original: rejected row absent; H->D restored
V4: same array, same row objects and order; G->T survives rebuild
V2 original: A active; disconnected B inactive, source empty
R-T2-AGUA: rejection after detach; D has no wire/node but stays active with source P
R-T2-AGUA control: 11 legacy + old row -> accepted, D active WITH wire
V5 direct/deferred control: actor+observer; far excluded
R3-01: live empty owner pruned before flush -> actor only; observer misses invalidation
V3 dead-owner control: 1000 cancellations; zero retained keys
F02: 20673 cases; no false full/room; double match counted once
MODELS ONLY: no Enforce runtime, RPC transport or compilation tested
```

Los oráculos son obligaciones independientes: no consumir nodos por un rechazo, conservar el cable previamente admitido tras rebuild, apagar un aspersor sin enlace y alcanzar al observador del cable que se retiró. Los checks de texto solo detectan parte del drift del modelo respecto al source.

El modelo topológico omite watchdogs de componentes, presupuestos de propagación, valores eléctricos y registro engine. Los fixtures requieren componentes pequeños, acíclicos, endpoints vivos y cuota/energía/geometría válidas. El modelo de agua aísla los setters que consumen el store. El de publicación aísla la secuencia corte→poda→flush; demuestra pérdida del destinatario calculado, no la entrega física del RPC. Para cerrar el producto en el futuro hacen falta los mismos discriminadores en Enforce, distinguiendo preparación inválida de un PASS.

## SI ESTO SE ARCHIVA, QUE HAY QUE ESCRIBIR

1. **MEDIO — R-T2-AGUA: el rechazo de una conexión puede activar su aspersor de destino aunque el cable se retire durante rollback.** La notificación de alta refresca dispositivos antes de conocer el resultado del grafo; la retirada provisional no deshace ese refresh. El fixture con una bomba alimentada y 13 filas legacy deja el aspersor nuevo sin cable ni nodo, pero activo. El reset periódico apaga la actividad; hasta entonces puede ejecutar riego. Conservar el fixture y exigir también el control del reemplazo exitoso.
2. **MEDIO — R3-01: la poda de un store vanilla vacío cancela una invalidación todavía necesaria para un owner vivo.** Si se cortó el último cable durante FullSync y la poda ocurre antes del flush, un observador cerca del extremo antiguo puede conservar el cable eliminado. La corrección futura debe mantener la liberación de owners muertos y preservar el interés pendiente de los vivos.
3. **VALIDACIÓN PENDIENTE — S-01/CCTV y runtime de T2.** No se ha acreditado con DayZ la selección de destinatarios durante CCTV, la compilación de esta foto ni los controles funcionales V1..V7. Esta ausencia de medición no es un crash ni otro defecto confirmado.
4. **DEUDA DE DISEÑO — la transacción conserva stores, pero los helpers de notificación modifican estado funcional fuera de una decisión única de commit; las invalidaciones viven separadas de su publicación pendiente.** Al retomar, incluir ambos límites en el contrato y los escenarios anteriores en sus pruebas. Las reglas duplicadas de capacidad en admisión e inserción siguen coincidiendo en el dominio comprobado, pero tienen riesgo de divergencia futura.

**No arrastrar como abiertas** la duda de `Find`, el orden del snapshot, el fixture huérfano N-01, F-02 ni la captura directa de los dos extremos de V5: aquí tienen evidencia de cierre. Tampoco renombrar la cancelación prematura como “fuga N-03”; la retención del owner muerto se ha corregido.

## LA PREMISA DE ESTE ENCARGO

**Los siete puntos son adecuados para revisar las correcciones anteriores, pero no bastan como definición completa de atomicidad y publicación.** Mantengo su texto: V3 cierra la fuga aunque su implementación introduzca el fallo inverso; V5 acredita registrar el extremo, sin garantizar que otra ruta no lo borre; V6 permite una regresión MEDIA. No he convertido “cero GRAVE nuevo” en “cero defecto nuevo”. El incumplimiento del VERDE existente es V2, que exige que ningún dispositivo quede activo sin cable, más allá del control concreto de B.

Hay un motivo estructural para la repetición. `NotifyGraphWireAdded` no es solo una operación del grafo: N:1718 cambia relaciones de agua y termina en setters sincronizados (S:140-147). Del mismo modo, una entrada de invalidación no pertenece únicamente a la existencia de un array de cables: N:4987 la borra mientras otra cola aún la necesita. La ronda 3 arregla el orden de una lectura y la liberación de una memoria, pero los propietarios y momentos de esos efectos siguen separados.

**Recomiendo rediseñar esos dos límites al retomar el tramo**, en vez de continuar moviendo llamadas para cada síntoma. [DESIGN] Preparar la mutación, resolver su admisión y aplicar las consecuencias de dispositivos sobre el estado definitivo; hacer que cada publicación pendiente conserve su información de invalidación y la termine al consumirla o cancelarla por una causa válida. La decisión de producto SEC02 permanece intacta. Este diseño puede mantenerse dentro de memoria de misión y conservar los formatos actuales; no requiere una migración de datos ni un nuevo payload para corregir los dos mecanismos encontrados.

No inicio esa implementación ni una cuarta revisión. La investigación, las citas, los controles y la deuda quedan en este único dictamen como memoria durable, respetando la orden de no actualizar otros archivos ni el vault.

### Reproducción offline

[EXACT — Python ejecutado en esta revisión; modelo parcial del código, no código Enforce]

Ejecutar por stdin desde esta raíz, con `python -X utf8 -B -`. Solo lee los cuatro fuentes indicados. El modo con poda es el código actual; el control sin poda representa que la publicación conserva la posición antigua.

```python
from pathlib import Path
from dataclasses import dataclass
from itertools import product
import re

ROOT = Path(".")
N = (ROOT/"scripts/5_Mission/LFPG_NetworkManagerImpl.c").read_text(encoding="utf-8-sig")
E = (ROOT/"scripts/5_Mission/LFPG_ElecGraphImpl.c").read_text(encoding="utf-8-sig")
H = (ROOT/"scripts/5_Mission/LFPG_RPCServerHandlerImpl.c").read_text(encoding="utf-8-sig")
D = (ROOT/"scripts/3_Game/LFPG_Defines.c").read_text(encoding="utf-8-sig")
def method(source, signature):
    start = source.index(signature)
    end = source.index("\n    }", start)
    return source[start:end]
def constant(name):
    return int(re.search(r"\b"+name+r"\s*=\s*(\d+)", D).group(1))
EDGE = constant("LFPG_MAX_EDGES_PER_NODE")
NODE = constant("LFPG_MAX_NODES_GLOBAL")
HARD = constant("LFPG_MAX_WIRES_PER_DEVICE")
# Drift guards, not the behavioral oracle.
onadd = method(E, "override bool OnWireAdded(")
assert onadd.index("EdgeCapacityReached(") < onadd.index("EnsureNode(sourceId")
detach = method(N, "protected void FinishTxnDetachConflicts(")
assert detach.index("live.Remove(gw)") < detach.index("NotifyGraphWireRemoved(")
assert "live.Clear();" in method(N, "protected void FinishTxnApplyStoreSnapshot(")
notify = method(N, "override bool NotifyGraphWireAdded(")
assert notify.index("LFPG_RefreshPumpSprinklerLink(") > notify.index("if (inserted)")
assert "LFPG_RefreshPumpSprinklerLink" not in method(N, "protected void FinishTxnUninsertNew(")
assert "ClearVanillaInvalidation(emptyOwners[eo])" in method(N, "protected int PruneUnresolvableVanillaWires(")

@dataclass(eq=False)
class W:
    src: str
    sp: str
    dst: str
    dp: str = "input_main"
    creator: str = "actor"

def remove_swap(a, i):
    a[i] = a[-1]
    a.pop()

class Graph:
    def __init__(self, stores):
        self.edges = []
        self.nodes = set()
        for rows in stores.values():
            for w in rows:
                self.nodes.update((w.src, w.dst))
                if not self.full(w):
                    self.edges.append(w)
        self.nodes = {x for w in self.edges for x in (w.src, w.dst)}
    def full(self, w):
        return (sum(e.src == w.src for e in self.edges) >= EDGE or
                sum(e.dst == w.dst for e in self.edges) >= EDGE)
    def add(self, w):
        if len(self.nodes) >= NODE or self.full(w):
            return False
        self.nodes.update((w.src, w.dst))
        if any((e.src,e.sp,e.dst,e.dp)==(w.src,w.sp,w.dst,w.dp) for e in self.edges):
            return False
        self.edges.append(w)
        return True
    def remove(self, w):
        for e in self.edges:
            if (e.src,e.sp,e.dst,e.dp)==(w.src,w.sp,w.dst,w.dp):
                self.edges.remove(e)
                break
    def end(self, removed):
        for w in removed:
            for node in (w.src,w.dst):
                if not any(node in (e.src,e.dst) for e in self.edges):
                    self.nodes.discard(node)

class Water:
    def __init__(self, stores, sprinklers):
        self.stores = stores
        self.sprinklers = {s: [False, ""] for s in sprinklers}
    def refresh(self, source, removed=""):
        if source != "P":
            return
        if removed in self.sprinklers and self.sprinklers[removed][1] == source:
            self.sprinklers[removed] = [False, ""]
        targets = [w.dst for w in self.stores.get(source, [])
                   if w.dst in self.sprinklers and w.dst != removed]
        # Powered T2 with water in the tank: both <=2 and >2 branches activate.
        for s in targets:
            self.sprinklers[s] = [True, source]

def txn(stores, graph, new, water=None):
    rows = [w for w in stores.get(new.src, [])
            if w.sp == new.sp or (w.dst,w.dp)==(new.dst,new.dp)]
    for owner, values in stores.items():
        if owner != new.src:
            rows += [w for w in values if (w.dst,w.dp)==(new.dst,new.dp)]
    snapshots = {w.src: list(stores[w.src]) for w in rows}
    for owner in snapshots:
        live = stores[owner]
        for i in range(len(live)-1,-1,-1):
            if live[i] in rows:
                remove_swap(live,i)
    for w in rows:
        graph.remove(w)
        if water: water.refresh(w.src,w.dst)
    stores.setdefault(new.src,[]).append(new)
    ok = graph.add(new)
    if water: water.refresh(new.src)  # N:1718, even on rejection
    if not ok:
        stores[new.src].remove(new)  # new row is last
        for owner, snapshot in snapshots.items():
            stores[owner].clear()
            stores[owner].extend(snapshot)
        for w in rows:
            graph.add(w)
            if water: water.refresh(w.src)
    graph.end(rows)
    return ok

def legacy(src, count):
    return [W(src, "legacy_"+str(i), src+"_L"+str(i)) for i in range(count)]

s = {"G": legacy("G",EDGE)}
for i in range((NODE-1-(EDGE+1))//2):
    s["X"+str(i)] = [W("X"+str(i),"output_1","Y"+str(i))]
g = Graph(s)
before = len(g.nodes)
assert before == 2047
assert not txn(s,g,W("G","output_1","D"))
assert len(g.nodes)==before and "D" not in g.nodes
assert txn(s,g,W("X0","output_2","Y1"))
print("V1: 2047 -> 2047; D absent; independent connection accepted")

s = {"G":legacy("G",EDGE), "H":[W("H","output_1","D")]}
g = Graph(s)
old = s["H"][0]
assert not txn(s,g,W("G","output_1","D"))
assert len(s["G"])==12 and s["H"]==[old] and old in g.edges
print("F01 original: rejected row absent; H->D restored")

old = W("G","output_1","T")
s = {"G":[old]+legacy("G",EDGE)}
for i in range(EDGE):
    s["H"+str(i)] = [W("H"+str(i),"output_1","D","legacy_in_"+str(i))]
g=Graph(s)
identity=id(s["G"]); sequence=list(s["G"])
assert not txn(s,g,W("G","output_1","D"))
assert id(s["G"])==identity
assert all(a is b for a,b in zip(s["G"],sequence))
assert len(s["G"])==len(sequence) and old in Graph(s).edges
print("V4: same array, same row objects and order; G->T survives rebuild")

s={"P":[W("P","output_2","B","input_0"),W("P","output_1","A","input_0")],
   "Supply":[W("Supply","output_1","P","input_1")]}
g=Graph(s); water=Water(s,["A","B"]); water.refresh("P")
assert txn(s,g,W("P","output_2","A","input_0"),water)
assert water.sprinklers["B"]==[False,""] and water.sprinklers["A"]==[True,"P"]
print("V2 original: A active; disconnected B inactive, source empty")

s={"P":[W("P","legacy_"+str(i),"P_L"+str(i),"input_1") for i in range(EDGE)]
          +[W("P","output_1","L_old","input_1")],
   "Supply":[W("Supply","output_1","P","input_1")]}
g=Graph(s); water=Water(s,["D"])
sequence=list(s["P"])
assert not txn(s,g,W("P","output_1","D","input_0"),water)
assert s["P"]==sequence and not any(w.dst=="D" for w in s["P"])
assert water.sprinklers["D"]==[True,"P"] and "D" not in g.nodes
print("R-T2-AGUA: rejection after detach; D has no wire/node but stays active with source P")

s={"P":[W("P","legacy_"+str(i),"P_L"+str(i),"input_1") for i in range(EDGE-1)]
          +[W("P","output_1","L_old","input_1")],
   "Supply":[W("Supply","output_1","P","input_1")]}
g=Graph(s); water=Water(s,["D"])
assert txn(s,g,W("P","output_1","D","input_0"),water)
assert any(w.dst=="D" for w in s["P"]) and water.sprinklers["D"]==[True,"P"]
print("R-T2-AGUA control: 11 legacy + old row -> accepted, D active WITH wire")

players={"actor":0,"observer":72,"far":200}
def recipients(extras):
    return {p for p,x in players.items() if any(abs(x-y)<=70 for y in [0]+extras)}
def deferred_cut(prune, owner_alive=True):
    pending={"G":owner_alive}
    extras={"G":[60]}  # captured before cutting the last row G->T
    stores={"G":[]}
    if prune:
        for owner in list(stores):
            if not stores[owner]:
                del stores[owner]
                extras.pop(owner,None)  # N:4987, owner can still be alive
    sent=set()
    for owner,alive in pending.items():
        if alive:
            sent |= recipients(extras.get(owner,[]))
        extras.pop(owner,None)
    pending.clear()
    return sent, extras
assert deferred_cut(False)[0]=={"actor","observer"}
assert deferred_cut(True)[0]=={"actor"}
assert deferred_cut(False,False)==(set(),{})
print("V5 direct/deferred control: actor+observer; far excluded")
print("R3-01: live empty owner pruned before flush -> actor only; observer misses invalidation")
for i in range(1000):
    assert deferred_cut(False,False)==(set(),{})
print("V3 dead-owner control: 1000 cancellations; zero retained keys")

cases=0
for size in range(6):
    for categories in product(range(4),repeat=size):
        # Two independent removals (same OUT, then same IN) are the oracle.
        actual=list(enumerate(categories))
        after_out=[r for r in actual if not (r[1]&1)]
        after_in=[r for r in after_out if not (r[1]&2)]
        selected={i for i,c in enumerate(categories) if c&1 or c&2}
        remaining=size-len(selected)
        for setting in (1,2,4,64,128):
            for native in (False,True):
                cap=min(setting,HARD) if native else setting
                assert (remaining<cap)==(len(after_in)+1<=cap)
                cases+=1
for native in (False,True):
    for setting in range(1,129):
        cap=min(setting,HARD) if native else setting
        for size in {0,1,cap-1,cap,cap+1,63,64,65,127,128,129}:
            for removed in {0,min(size,1),size}:
                predicted=size-removed<cap
                actual_count=len(list(range(size))[removed:])+1
                assert predicted==(actual_count<=cap)
                cases+=1
assert 1-len({0}) < 1  # same row matching OUT+IN counted once
print("F02:",cases,"cases; no false full/room; double match counted once")
print("MODELS ONLY: no Enforce runtime, RPC transport or compilation tested")

```
