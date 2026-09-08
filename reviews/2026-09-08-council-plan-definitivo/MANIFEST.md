# MANIFEST — council plan definitivo LFPowerGrid

tipo: **COBERTURA**

Justificacion del tipo: las seis lanes reciben el **mismo brief byte-identico**, pero sus
capacidades de lectura y sus techos de contexto difieren de forma material (una via `codex exec`
con shell, tres via `cursor-agent --mode ask` sin shell, una CLI de Gemini, un subagente
Anthropic in-process). Comparar su PRECISION entre si seria el fallo F1 reetiquetado. La
pregunta que responde esta corrida es **"que se nos escapa del solape auditoria<->trabajo ya
triado"**, no "que proveedor acierta mas". Prohibido rellenar precision-por-proveedor con esta
corrida.

- **run_id:** `council-lfpg-plan-20260908`
- **fecha:** 2026-09-08
- **pregunta:** Plan de ejecucion definitivo para lo que queda en LFPowerGrid, partido en lanes
  paralelas disjuntas por fichero, adjudicando primero cuanto de la auditoria 2026-09-07 es
  trabajo nuevo.
- **brief_path:** `P:\LFPowerGrid\reviews\2026-09-08-council-plan-definitivo\BRIEF.md`
- **brief_sha256:** `abed2152bb85ef7d47c77fdb796928064e648ab5f1b747c8c8395546f049d707`
- **brief mtime:** 2026-09-08 12:50:53 (anterior al primer PID; re-sellado tras traer las
  dos fuentes externas a `<RUN>/fuentes\`, ANTES de lanzar nada.
  El sello viejo `8ea300ec...` queda invalidado y no lo recibio ninguna lane.)

## N = 6, y por que (C2 exige justificarlo por encima de 2)

**decision_de_producto:** *¿el fork `_TEST` del sorter sale del release?* La auditoria le pone
un 3/10 y propone sacarlo; el arbol lo tiene spawneable en `config.cpp` y ligado a persistencia
por herencia. Segun se decida, una fraccion grande de las 92 fichas vivas deja de importar o se
convierte en un refactor de base comun. **No es una decision que el orquestador pueda tomar
leyendo el arbol**: depende de si el dueño quiere seguir enviando dos sorters.

**coste esperado vs hallazgo unico esperado:** 6 lanes de lectura pesada (~22 KB de auditoria +
5 informes de triaje + 8 dictamenes). El hallazgo unico que se busca es la **tabla de solape**:
46 hallazgos contra ~110 items ya triados, un cruce que nadie ha hecho y que decide si el
backlog real son 170 items o 60. Una sola lectura (la mia) tiene un modo de fallo conocido —
declarar NUEVO lo que ya esta cubierto, porque no recuerdo las 92 fichas de memoria.

⚠ **Reserva declarada:** el protocolo (`C2`) dice que **un council no planifica un plan**. Se
abre igualmente por peticion explicita del usuario (2026-09-08) y porque el objeto real de la
corrida es el SOLAPE, que si es un problema de cobertura. Si el resultado confirma que el
solape era adivinable de una lectura, esta corrida cuenta como evidencia a favor de C2.

## Lanes

| id | proveedor | binario / via | modelo_pin | tools | puede_leer_arbol | cuota_pre | output_path |
|---|---|---|---|---|---|---|---|
| `fable` | Anthropic | Agent tool, subagente in-process | `fable` | Read/Grep/Glob/Bash | **si** | n/a (sin cuota externa), 12:45 | `lanes\fable\PLAN.md` |
| `astra` | OpenAI | `codex exec` | `gpt-6-astra` | shell + fs completos | **si** | **6,0 %** de ventana semanal (10080 min), plan pro; leido 12:44 del rollout `01a0803a` | `lanes\astra\PLAN.md` |
| `grok` | xAI via Cursor | `cursor-agent --mode ask --trust` | `cursor-grok-4.6-xhigh` | lectura del workspace | **si** | tarifa plana Ultra, sin contador; binario presente 12:45 | `lanes\grok\PLAN.md` |
| `kimik3` | Moonshot via Cursor | `cursor-agent --mode ask --trust` | `kimi-k3-max` | lectura del workspace | **si** | tarifa plana Ultra; sin contador | `lanes\kimik3\PLAN.md` |
| `glm` | Z.ai via Cursor | `cursor-agent --mode ask --trust` | `glm-5.2-max` | lectura del workspace | **si** | tarifa plana Ultra; sin contador | `lanes\glm\PLAN.md` |
| `gemini` | Google | `gemini` CLI | `gemini-3.7-flash` (a confirmar por oraculo) | lectura con `--include-directories` | **si** | free tier, contador no leido sin generar | `lanes\gemini\PLAN.md` |

### Sustituciones respecto a lo que pidio el usuario, declaradas (C13)

- **Grok NO va por `grok-cli`.** Esa celda esta `BROKEN` por saldo **dos veces** (2026-08-31 y
  2026-09-06, las dos con `402 Payment Required: Grok Build usage balance exhausted` antes del
  primer turno). Sustituto medido: `cursor-grok-4.6-xhigh` x `cursor-cli`, `WORKS 2026-09-01`
  con 10 corridas reales. Fila **`SUBSTITUTED`** — es el mismo modelo por otra puerta, no otro
  modelo.
- **GLM es `glm-5.2-max`, no 5.3.** `glm-5.3` x `zcode-app` esta `BROKEN` (app KO, 2026-08-31);
  x `bai` da `403 Deposit required` y solo el `flash` es gratis; x NIM es `UNAVAILABLE`
  (0 coincidencias en el catalogo). La unica celda GLM medida y viva hoy es la 5.2 por Cursor.
  Fila **`SUBSTITUTED`**, y aqui **si cambia el modelo**: no se compara con corridas historicas
  de 5.3.
- **Kimi K3 va por Cursor, no por NIM.** `moonshotai/kimi-k3` x NIM murio **5/5 por 429** el
  2026-08-31 y su fila avisa de que el racionamiento puede durar la sesion entera.

### ⚠ Concentracion por puerta (LL-449)

**3 de 6 lanes entran por `cursor-agent`** (`grok`, `kimik3`, `glm`). Es exactamente la mitad
del council por una sola puerta: si Cursor cae, caen tres lanes a la vez y N pasa a 3. Se
declara en vez de fingir seis puertas independientes. Recuento post-sustitucion: Anthropic 1,
OpenAI 1, Cursor 3, Google 1.

⚠ **`arbitro=lane` a nivel de familia:** el arbitro es Claude (orquestador) y `fable` es
tambien Anthropic. Toda fila del consolidado que dependa de `fable` se marca en el arbitraje.

## fallback

Ninguno declarado. Si una lane muere, **N se reduce y se dice** — no se sustituye en silencio
ni se completa de memoria. Minimo viable para cerrar la corrida: 3 lanes `COMPLETE` de al menos
2 familias distintas.

## universo_de_cifras

Toda cifra del brief §4, con su censo:

| cifra | universo | como se conto |
|---|---|---|
| 101 fichas = 92 VIVA + 6 DUDOSA + 3 MUERTA | los 5 `TRIAJE-*.md` de `reviews\2026-09-08-triaje-fichas-p2-p3\` | regex sobre la 1ª columna de las tablas de veredicto: `^\|\s*([A-Z]{1,4}\d{2})\s*\|\s*(VIVA\|MUERTA\|DUDOSA)` |
| 46 hallazgos de auditoria | `LFPowerGrid_Auditoria_2026-09-07.md` | 34 numerados por regex `^(?:- \*\*\|### )([A-Z]{1,3}\d+)` dentro de §1-§4, + 5 viñetas de §5 + 7 numeradas de §6, contadas a mano |
| 9 MEDIO / 11 MENOR | los 8 `DICTAMEN-GROK.md` de las dos oleadas | cabeceras `^#{2,4}.*\b(MEDIO\|MENOR)\b`; MEDIO = 8 de oleada 1 − 1 arreglado en `d59cad8` + 2 de `l7` |
| 7 CONFLICTO | `l7-debt-v3\INFORME-ASTRA.md` | filas con `CONFLICTO` en su tabla de decision |
| 263 ficheros / 0 errores / 47 warnings | arbol `main` @ `e5b5303` | `script_validator.py .`, JSON, claves `errors`/`warnings` en raiz |
| 1.162 inserciones netas de `archive/t2` | `git diff main...archive/t2-autoridad-servidor --stat -- scripts/` | 4 ficheros, `1162 insertions(+), 184 deletions(-)` |
| 6.989 lineas de `NetworkManagerImpl` | cifra **heredada de la auditoria**, no re-contada por el orquestador | `[SIN CENSO PROPIO]` |

## arbitraje

- **arbitro:** Claude (orquestador de esta sesion). Marca `⚠ arbitro=lane` en toda fila
  sostenida por `fable`.
- **regla:** evidencia sobre mayoria. Dos lanes de acuerdo sin `path:line` no vencen a una con
  cita re-verificada por el orquestador. Disenso fuerte se copia **verbatim** al usuario.
- **antes de rankear:** el orquestador re-abre las citas load-bearing de la tabla de solape.
  Una ficha declarada "cubierta" que no lo este es un `RECHAZADO-FALSO`.

## presupuesto de rondas

**Una ronda ciega.** No hay ronda 2 de debate: el tipo es `COBERTURA`, y el refinamiento
contamina. El consolidado lo escribe el orquestador y lo aprueba el usuario antes de que se
gaste un solo subagente de ejecucion.
