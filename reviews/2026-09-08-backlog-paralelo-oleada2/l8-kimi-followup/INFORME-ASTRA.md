# Rescate de maint/audit-kimi-followup

| Identificador original | Fichero | Decisión | Evidencia en el árbol entregado |
|---|---|---|---|
| M-01 | scripts/3_Game/LFPG_Telemetry.c | PORTAR | scripts/3_Game/LFPG_Telemetry.c:177, scripts/3_Game/LFPG_Telemetry.c:193, scripts/3_Game/LFPG_Telemetry.c:197, scripts/3_Game/LFPG_Telemetry.c:220 |
| M-02 | scripts/4_World/LFPG_CableRenderer.c | PORTAR | scripts/4_World/LFPG_CableRenderer.c:999 y scripts/4_World/LFPG_CableRenderer.c:1003: en el hueco eliminado queda directamente el resolver activo |
| M-03 | scripts/3_Game/LFPG_Migrators.c | PORTAR | scripts/3_Game/LFPG_Migrators.c:4, scripts/3_Game/LFPG_Migrators.c:56 y scripts/3_Game/LFPG_Migrators.c:82 |

Fecha: 2026-09-08. Los identificadores proceden del mensaje del commit de origen, no del orden de ficheros del brief.

Base efectiva: HEAD y main eran `d59cad892557d8ec8dcfed0bfca0ba1c5744db45`, en `lane/l8-kimi-followup`. Rama consultada: `maint/audit-kimi-followup`, commit `1d178ec3279b0fbf2e8f981adca2e3a03f3e3442`; ancestro común `289592b6312d42a85615e487eeb900b48b0276b7`. Se localizaron los tres cambios por contenido mediante log, diff de tres puntos y lectura de fuentes actuales. No se integró la rama ni se reemplazaron ficheros con sus versiones antiguas.

Cambios entregados: tres scripts, **11 líneas añadidas / 86 eliminadas**. M-01: +4/-4; M-02: +0/-56; M-03: +7/-26. `INFORME.md` es el entregable adicional expresamente solicitado. No se modificaron otros ficheros ni el índice; no hay commit por prohibición del encargo. `BRIEF.md`, `EXIT.start`, `events.jsonl` y `stderr.log` ya estaban presentes al comenzar y no se editaron.

### M-01 — Resúmenes de telemetría condicionados

**Veredicto: ARREGLADA. Decisión: PORTAR.**

Se añadió `LFPG_PERFDIAG_ENABLED` a las dos condiciones ya existentes, en `scripts/3_Game/LFPG_Telemetry.c:177` y `scripts/3_Game/LFPG_Telemetry.c:197`. Las dos emisiones pasan por `LFPG_Util.Info`, en `scripts/3_Game/LFPG_Telemetry.c:193` y `scripts/3_Game/LFPG_Telemetry.c:220`. No se añadió una envoltura que obligara a reindentar ambos bloques completos.

La definición de la bandera está en `scripts/3_Game/LFPG_Defines.c:405`, dentro del mismo módulo base. `LFPG_Util.Info(string msg)` está definido en `scripts/3_Game/LFPG_Util.c:16`; su implementación común en `scripts/3_Game/LFPG_Util.c:7` aplica el prefijo y los controles de logging. Con los valores actuales (`scripts/3_Game/LFPG_Defines.c:398`, `scripts/3_Game/LFPG_Defines.c:399`, `scripts/3_Game/LFPG_Defines.c:400`) conserva el texto y prefijo de los resúmenes al activar PERFDIAG.

Decisión conservadora: usar Info, que funciona con el nivel actual 1. Se descartó Debug porque exigiría además subir el nivel a 2, y se descartó copiar los Print de la rama porque el brief los prohíbe. Diferencia intencionada respecto al port literal: con logging desactivado o nivel inferior a 1, estos resúmenes también quedan silenciados aunque PERFDIAG esté activo.

Los acumuladores, el reset por frame (`scripts/3_Game/LFPG_Telemetry.c:153`) y el reset por intervalo (`scripts/3_Game/LFPG_Telemetry.c:223`) siguen fuera de esas condiciones. No se condicionó Tick ni se retiraron los objetos de métricas. Hay consumidores reales: `scripts/4_World/LFPG_WiringClient.c:795` obtiene las métricas y `scripts/4_World/LFPG_WiringClient.c:1038` lee los contadores para su diagnóstico, regulado por DIAG en `scripts/4_World/LFPG_WiringClient.c:628`; `scripts/5_Mission/LFPG_MissionInit.c:431` llama Tick después del trabajo de render/preview. DIAG y PERFDIAG son independientes.

Borrados: se sustituyeron dos condiciones y dos llamadas de logging; no se retiró ningún símbolo en esta ficha y no hay llamadores huérfanos que buscar. Se descartó eliminar la telemetría como supuesto aparato sin consumidor.

Verificación realizada: lectura de esos productores/consumidores, inspección del diff de cuatro líneas y validador estático del fichero, exit 0, sin errores ni avisos. No es evidencia de ejecución de los contadores.

Verificación in-game pendiente: con cables y preview activos durante más de dos intervalos, comprobar ausencia de ambos resúmenes con PERFDIAG=false; con PERFDIAG=true, LOG_ENABLED=true y LOG_LEVEL=1, comprobar ambos resúmenes, cadencia aproximada de 5 segundos y valores por intervalo. Repetir DIAG=true/PERFDIAG=false y verificar que los contadores del diagnóstico de preview no se acumulan entre frames. Comprobar que sin spans/cables no se generan resúmenes y que LOG_ENABLED=false los suprime. No se cambiaron las banderas del repositorio para estas pruebas.

### M-02 — Retirada exclusiva del resolver antiguo

**Veredicto: ARREGLADA. Decisión: PORTAR.**

Se retiró únicamente `ResolveDeviceEntity(string deviceId)`, con su cuerpo y línea separadora: exactamente las 56 líneas propuestas por la rama. El hueco se puede abrir en `scripts/4_World/LFPG_CableRenderer.c:999`; la siguiente definición es `ResolveDeviceEntityEx` en `scripts/4_World/LFPG_CableRenderer.c:1003`.

Antes de borrar, la búsqueda de `\bResolveDeviceEntity\b` en todo el árbol encontró solo su definición, entonces en la línea 999. La búsqueda separada del nombre Ex localizó sus tres llamadas, que permanecen en `scripts/4_World/LFPG_CableRenderer.c:1962`, `scripts/4_World/LFPG_CableRenderer.c:2062` y `scripts/4_World/LFPG_CableRenderer.c:3885`. La búsqueda por palabra completa evita confundir los dos nombres.

Prueba posterior del borrado: el comando de búsqueda reproducible de abajo, con el símbolo `ResolveDeviceEntity`, devolvió **exit 1, ninguna coincidencia, sin errores**. Incluye scripts de los tres módulos, configuración, archivos ocultos/ignorados y referencias textuales a callbacks. No queda ningún llamador textual en el árbol inspeccionado; no se extiende esta conclusión a mods externos ni a nombres construidos dinámicamente.

Se descartó retirar la caché negativa o su estado: `m_NegCache` se conserva en `scripts/4_World/LFPG_CableRenderer.c:770`, su lectura sigue en `scripts/4_World/LFPG_CableRenderer.c:1040` y el consumo de `m_LastResolveWasNegCached` sigue en `scripts/4_World/LFPG_CableRenderer.c:3894`. Son partes usadas por el resolver activo y sus reintentos.

La comparación binaria contra HEAD confirmó que **todos los bytes fuera de las 56 líneas eliminadas son idénticos**. Por tanto, permanecen íntegros el recálculo owner-null (`scripts/4_World/LFPG_CableRenderer.c:2436`), la liberación en CullTick (`scripts/4_World/LFPG_CableRenderer.c:2533`), `ReleaseWireSegments` (`scripts/4_World/LFPG_CableRenderer.c:4186`) y la admisión espacial `ReserveWireSegments` (`scripts/4_World/LFPG_CableRenderer.c:4197`, llamada en `scripts/4_World/LFPG_CableRenderer.c:2182`). Esto verifica la localización del diff; no demuestra que esos algoritmos funcionen correctamente en juego.

Verificación realizada: búsqueda global antes/después, lectura del resolver activo y las zonas R02/R04, comparación de bytes y validador estático del fichero, exit 0, sin errores ni avisos.

Verificación in-game pendiente: resolver destino registrado, destino mediante NetworkID durante retraso de SyncVars y destino vanilla; comprobar reintento tras fallo y expiración de caché. Con owner fuera de streaming durante más de 30 segundos, moverse cerca/lejos de sus cables y observar la limpieza. Bajo presión del presupuesto, salir y volver al radio de visibilidad y comprobar liberación y reconstrucción. Son pruebas de regresión del camino conservado; el método borrado no tiene ruta invocable encontrada en este árbol.

### M-03 — Integrar los logs de los auxiliares de migración

**Veredicto: ARREGLADA. Decisión: PORTAR.**

Se retiraron los cuerpos de `MigrateV1ToV2` y `MigrateVanillaV1ToV2` y sus comentarios asociados. Cada cuerpo contenía únicamente un Info. Esos mismos literales y llamadas están ahora en el punto de invocación, `scripts/3_Game/LFPG_Migrators.c:56` y `scripts/3_Game/LFPG_Migrators.c:82`, antes de la misma asignación de versión. Permanecen las entradas públicas `MigrateBlob` (`scripts/3_Game/LFPG_Migrators.c:39`) y `MigrateVanillaStore` (`scripts/3_Game/LFPG_Migrators.c:67`).

Se corrigieron la cabecera (`scripts/3_Game/LFPG_Migrators.c:4`) y la descripción del método (`scripts/3_Game/LFPG_Migrators.c:37`): estos auxiliares no validan datos ni prueban que hayan sido saneados. No se portó la frase de la rama que interpreta el incremento de versión como prueba de haber pasado por sanitización.

Prueba de los dos borrados: antes del cambio, `MigrateV1ToV2` solo aparecía en su llamada interna y definición, y lo mismo ocurría con `MigrateVanillaV1ToV2`. Después, el comando de abajo, ejecutado para cada nombre por separado, devolvió **exit 1, ninguna coincidencia, sin errores**. Se buscó en todo el árbol, no solo en 3_Game; no quedan llamadas textuales a los auxiliares eliminados.

Además, la búsqueda global de `\b(MigrateBlob|MigrateVanillaStore)\b` solo devuelve hoy las dos declaraciones públicas. La retirada de la cadena del cargador está documentada y visible en `scripts/3_Game/LFPG_WireHelper.c:321`; la validación real se invoca en `scripts/3_Game/LFPG_WireHelper.c:369` y `scripts/5_Mission/LFPG_NetworkManagerImpl.c:4672`. Se conserva la API pública por el alcance de M-03. Se descartó conectarla de nuevo a los cargadores, borrar toda la clase o implementar una migración v2→v3: serían cambios adicionales de contrato/persistencia.

Verificación realizada: lectura y comparación de las rutas null, versión anterior, actual y futura. Se mantienen los guards, returns, asignaciones de `.ver` y literales de log. No hay cambios de esquema ni de ficheros de datos, por lo que esta ficha no necesita migrar ni revertir datos. Validador estático del fichero: exit 0, sin errores ni avisos. La comparación es de fuente, no una prueba ejecutada de migración.

Verificación in-game pendiente: como el juego no llama estas entradas en el árbol actual, cargar una partida por sí solo no ejercita M-03. Hace falta una sonda temporal que invoque ambas entradas con null y versiones 1, 2, 3 y 4; comparar retorno, versión, campos de cables y logs. El comportamiento preservado es v1→2, v2 sin cambios, v3 sin cambios y futuras sin descenso; null devuelve la constante correspondiente (Blob=3, Vanilla=2, `scripts/3_Game/LFPG_Defines.c:254` y `scripts/3_Game/LFPG_Defines.c:255`). La sonda no forma parte de los cambios entregados. El reinicio con persistencia normal sigue siendo una comprobación de regresión, no cobertura de los auxiliares.

## Evidencia reproducible y alcance estático

Búsqueda antes y después, desde la raíz. Sustituir SYMBOL por cada uno de `MigrateV1ToV2`, `MigrateVanillaV1ToV2` y `ResolveDeviceEntity`:

```powershell
rg -n -uuu -g '!.git' -g '!.git/**' -g '!events.jsonl' -g '!stderr.log' -g '!INFORME.md' '\bSYMBOL\b' .
```

Las únicas exclusiones son metadatos Git, los logs del arnés que registran nuestras propias consultas y este informe que cita los nombres retirados. No se excluyen scripts, configuración, reviews ni BRIEF.md. Resultado final de cada búsqueda: cero coincidencias, exit 1. Antes de borrar, los resultados eran respectivamente dos, dos y una coincidencia, todas dentro de los tres ficheros autorizados.

Validador existente ejecutado por separado para cada uno de los tres ficheros, con esta forma de comando:

```powershell
python -B 'C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py' 'scripts/3_Game/LFPG_Migrators.c'
```

Mismo comando con LFPG_Telemetry.c y scripts/4_World/LFPG_CableRenderer.c: **tres exit 0, status PASS, cero errores y cero avisos**, un fichero inspeccionado en cada ejecución. El primer intento global emitió WARN y terminó con código 1 tras enviarse una interrupción; su salida quedó truncada y no se cuenta como un resultado global adjudicado. El resultado entregado se limita a los tres scripts modificados.

`git diff --check` terminó sin incidencias. Relectura UTF-8 sin BOM ni bytes NUL y con CRLF exclusivo en los tres scripts: Migrators 89 líneas; Telemetry 242; CableRenderer 4350. Las líneas añadidas con indentación usan tabs; la indentación ajena se preservó. No se añadieron métodos de motor, referencias ascendentes entre módulos, RPC ni cambios de datos cliente/servidor: migradores/utilidades permanecen en 3_Game y la telemetría y el renderer conservan sus fronteras cliente. No se creó ni ejecutó un build ni el gate específico del receptor.

SHA-256 de los scripts entregados:

| Fichero | SHA-256 |
|---|---|
| scripts/3_Game/LFPG_Migrators.c | a2602b1ed00ac74dce1608bdd3b79d52a95e42c9e6c48228abdff26d7d760df7 |
| scripts/3_Game/LFPG_Telemetry.c | 37cb4705a7cde0c708c54a618603f0d94393f50ad691e0ed85bfe3e08eb841bb |
| scripts/4_World/LFPG_CableRenderer.c | 5163e55aae00cc132edd3652515514a2409a225e777dfee7daaa9d9e76a5a972 |

Este informe es el handoff de la lane. No se actualizó memoria durable ni se escribió en el vault porque el brief limita todas las escrituras al workspace y a estos entregables. La revisión de otra familia y la integración pertenecen al receptor.

## LO QUE NO PUDE VERIFICAR

- Compilación real de Enforce: requiere cargar el mundo; no se ejecutó juego ni se afirma que compile.
- Comportamiento de telemetría, render, caché, streaming y presupuesto con entidades reales; quedan los escenarios in-game de M-01 y M-02.
- Ejecución de los migradores mediante una sonda y carga/reinicio con datos persistidos; la ruta normal no invoca la API conservada.
- Consumidores en mods externos, binarios no incluidos o nombres de método construidos dinámicamente; la evidencia de ausencia se limita a referencias textuales en este árbol.
- Correctitud completa del repositorio, sus avisos globales y revisión independiente: solo se adjudicó el linter de los tres scripts de la lane.
- Mejora de memoria, tiempo de carga o rendimiento: quitar líneas sin consumidor no demuestra un ahorro medible.

## QUE PUEDE ESTAR MAL EN LA PREMISA DE ESTE ENCARGO

- La base indicada está desfasada: este worktree y main estaban en d59cad8, no en 8de29d5; la divergencia medida desde la rama origen hasta HEAD es de 26 commits, no 15. Se usó el estado real y se localizaron los bloques por contenido.
- Los tres items no son todos aparato sin consumidor. La telemetría sí tiene productores y lectores activos; quitar Tick o sus resets rompería el contrato que se pretendía conservar. M-01 solo debe silenciar sus resúmenes.
- M-03 reduce auxiliares, pero no restaura ninguna migración de producción: las entradas públicas tampoco tienen consumidores actuales. La cabecera antigua decía que DeserializeJSON las llamaba; la rama corregía eso, pero añadía otra afirmación no demostrada sobre haber pasado sanitización. Se evitó ambas afirmaciones. PERSIST_VER ya es 3 y este helper legado solo eleva versiones inferiores a 2 hasta 2; se conserva ese comportamiento, no se certifica soporte integral de migración a v3.
- El riesgo advertido en CableRenderer era real a nivel de fichero, pero no hay colisión en el bloque concreto: los cambios nuevos y los tres consumidores usan el resolver Ex. La ausencia de llamadores del resolver antiguo se comprobó; no se dedujo de que el borrado pareciera pequeño. La caché sigue siendo necesaria.
- Un port literal de M-01 introduciría Print y reindentaría docenas de líneas. Se conservaron las condiciones y el formato actuales, añadiendo la bandera en dos puntos y pasando el log por la utilidad exigida; esto hace que los resúmenes respeten también LOG_ENABLED/LOG_LEVEL.
- La lista blanca dice literalmente solo tres ficheros, pero el mismo encargo exige INFORME.md en disco. Se interpretó esa exigencia como excepción documental expresa, sin crear otros artefactos. No hay aprobación adicional pendiente.
