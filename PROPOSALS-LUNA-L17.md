# Propuestas de reduccion de LOC — Luna L17

Base: `033c08c`, `scripts/4_World/LFPG_IDevice.c` (978 lineas). Inventario tras leer completo el archivo y buscar referencias de llamadas en `scripts/`. La busqueda muestra que no hay helpers publicos evidentemente muertos; `CallInt/Bool/String/Vector/Void/Float` se usan dentro de este archivo y los demas metodos tienen consumidores en el repo.

Estimaciones de ahorro neto, contando lineas de codigo; los comentarios se anotan aparte. Ordenadas de mayor a menor.

| Prioridad | Propuesta | LOC | Estado | Motivo / riesgo |
|---|---|---:|---|---|
| 1 | Simplificar `IsElectricDevice`: conservar el guard `!e` y el ID LFPG, pero devolver directamente `IsVanillaSource(e) || IsVanillaConsumer(e)` en vez de dos bloques `if (...) return true` y el `return false` final. | 6 | APLICADA | Mismo orden de evaluacion y cortocircuito: la segunda comprobacion solo ocurre si la primera es falsa. |
| 2 | Simplificar `IsEnergySource`: tras el guard nulo, devolver `IsSource(e) || IsVanillaSource(e)` en vez de dos ramas verdaderas y `return false`. | 7 | APLICADA | Mismo orden/cortocircuito. |
| 3 | En `GetPortCount`, unir los dos fallbacks vanilla que devuelven `1` en un solo `if (IsVanillaSource(e) || IsVanillaConsumer(e)) return 1;`. | 2 | APLICADA | Conserva prioridad fuente y cortocircuito. |
| 4 | Eliminar comentarios de historial de version/correccion que describen cambios pasados ya representados por el codigo: anotaciones `v4.0`, `v4.6`, `v4.8`, `v4.9`, `v5.2`, `v0.7.x` en bloques internos (deteccion, energia, capacidad, fast-path, compatibilidad). | ~67 | DIFERIDA | Algunos contienen rationale funcional o excepciones no deducibles del codigo (p.ej. cargador, CompEM, passthrough, solar RaG); no es seguro borrar el conjunto sin una poda comentario por comentario y sin perder contexto operativo. |
| 5 | Quitar guardas nulas locales duplicadas cuando la primera operacion delegada ya es null-safe: wrappers `GetDeviceId`, `HasPort`, `IsSource`, `GetSourceOn`, `IsGate*`, `GetPowered`, `CanConnectTo`, wrappers de cable, y getters/setters de carga/sobrecarga/puertos/RF. | ~34 | DIFERIDA | En varias rutas la semantica nula difiere por fallback (`IsGateOpen` devuelve true; otros false/0/null); no se ha probado que `Cast`, los metodos nativos y el despacho dinamico sean todos equivalentes para null. |
| 6 | Acortar los `if/return` de una linea repetidos a expresiones compactas y eliminar llaves de ramas de una sola sentencia en los wrappers. | ~60 | DIFERIDA | Seria principalmente reformateo, con riesgo de inflar/complicar el diff y no constituye una redundancia demostrada. |
| 7 | Reescribir `ParseVanillaId` en bucle/ayudante para reducir los bloques repetidos de busqueda de separadores y `Substring`. | ~10-20 | DIFERIDA | Reduce texto a costa de nueva logica y puede cambiar el tratamiento de separadores vacios, sufijos extra o conversiones numericas; no es un corte de borrado seguro. |
| 8 | Quitar los wrappers `Call*` no referenciados por codigo externo. | 0 | DESCARTADA | No son muertos: hay usos dentro de `LFPG_DeviceAPI`; eliminarlos romperia consumidores internos. |
| 9 | Quitar metodos de API sin llamada textual detectada. | 0 | DESCARTADA | El barrido hallo llamadas a los metodos publicos; ademas el despacho dinamico/reflexion puede ser consumidor no textual. |
| 10 | Acortar comentarios largos de contrato/edge cases (ID vanilla, radio espacial 3D, autoridad de energia, orden NetworkID, APIs de cables). | variable | DIFERIDA | Documentan persistencia, orden RPC, fallos conocidos o limites de resolucion; no se eliminan sin demostrar que son obsoletos. |

## Aplicacion

Se aplican solo las tres simplificaciones booleanas de las filas 1–3. Sin extraccion de helpers, cambios de API ni modificaciones fuera del write-set.


