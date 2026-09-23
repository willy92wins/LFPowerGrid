# Propuestas L12 — LFPG_BTCAtmView

Inventario redactado tras leer el archivo completo (1325 líneas) antes de editar. Estimaciones de líneas netas, excluyendo cambios en blanco/comentarios salvo donde se indica.

| Rango | Recorte propuesto | LOC aprox. | Estado |
|---|---|---:|---|
| 183–1230 | Quitar las llaves de 39 `if`/`while` multilínea con exactamente una sentencia, conservando condición y orden. | 78 | Aplicado |
| 129–130 | Borrar `COL_STATUS_ERR_BG`, declarada pero nunca leída en el árbol `scripts/`. | 1 | Aplicado |
| 1090–1092 | En `FindButtonBg`, devolver directamente `ImageWidget.Cast(child)` tras el guard `!child`; eliminar variable local. | 0 neta | Aplicado (1 eliminación + 1 adición, limpieza sin ahorro LOC) |
| 1183–1193, 1205–1231 | Colapsar `OnTxResult` y `OnPriceUnavailable` a un helper común o una llamada compartida: ambos comprueban instancia/abierto/controlador y hacen exactamente `RefreshFromClientData`. Firmas públicas deben conservarse. | 7–10 | Diferido: helper agrega indirección y el ahorro es moderado frente al riesgo de alterar puntos de entrada visibles. |
| 1240–1288 | Sustituir variables de mensaje usadas una sola vez (`errMsg`, `openMsg`, `closeMsg`, init/warn) por argumento directo a logger. | 5 | Diferido: cambia forma de evaluación/consumo del argumento y no hace falta para el subconjunto seguro. |
| 328–362 | Eliminar variables locales de nombre asignadas inmediatamente antes de una sola llamada a `BindTabChildren`/`BindButtonChildren3`. | 12 | Diferido: transformación mecánica, pero líneas cercanas contienen nombres que son contratos de layout; conservar referencias explícitas facilita auditar el binding. |
| 246–397, 515–559 | Reducir el patrón repetido `wn = ...; if (!field) field = Cast(root.FindAnyWidget(wn));` y el patrón de asignación de IDs mediante helper de binding genérico. | 35–45 | Diferido: helper/abstracción de Enforce introduce riesgo de resolver widgets/casts distintos y no está justificado sin verificar contratos de layout. |
| 438–507 | Simplificar la asignación por nombre en `BindButtonChildren3` y `BindTabChildren` (comparaciones literales en vez de variables constantes locales; `else if` solo si se demuestra equivalencia). | 4–10 | Diferido: revisar por separado; las ramas actualmente son independientes y la cadena cambiaría la semántica solo si un nombre pudiera coincidir. |
| 790–793, 839–843, 974–1007 | Explorar eliminación de temporales/casts dentro de recorridos de padres/children. | 2–6 | Diferido: variables `check`, `btn`, `child` y casts sostienen iteraciones y retornos; no hay helper claramente menor y equivalente. |
| 717–729, 1030–1067 | Reducir temporales de color/clamp y retornos finales condicionados (por ejemplo `elapsed`). | 2–5 | Diferido: legibilidad de caminos numéricos/hover pesa más que el ahorro pequeño; algunas variables preservan la secuencia de lecturas del engine. |
| 223–229, 255–263, 405–411, 556–650 | Quitar comentarios de sección y comentarios históricos sin efecto runtime. | 30+ | Diferido: no son código muerto y varios documentan contratos/bugs previos; conservar trazabilidad técnica. |
| Campos enlazados (p.ej. `PriceChangeText`, `FooterBrand`) | Borrar referencias aparentemente no consumidas localmente. | variable | Diferido: otros archivos pueden consumir los campos públicos; no es seguro inferir inactividad desde este write-set. |
| `OnChange` y guards de callbacks | Eliminar retornos falsos/rechecks o fusionar condiciones. | 0–3 | Diferido: retornos forman parte del contrato del callback, y los guards separan estados válidos; ahorro neto no claro. |

Se priorizó retirar llaves sin alterar ramas/orden, junto con el único constante sin lecturas. Ninguna entidad, API pública ni contrato visual se elimina.
