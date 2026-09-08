# BRIEF — Plan definitivo de lo que queda en LFPowerGrid

Eres UNA lane de un council. Trabajas **a ciegas**: no ves lo que escriben las demas y no
debes buscarlo. Tu entregable se compara con el de ellas.

---

## 1. LA TAREA, EN UNA FRASE

Escribe un **plan de ejecucion definitivo** para todo lo que queda pendiente en el mod
LFPowerGrid, partido en **lanes lo mas paralelas posible** — lanes que un subagente de Codex
pueda ejecutar a la vez sin pisarse.

## 2. TU ROL Y EL CRITERIO DE EXITO

Rol: **autoria de plan**, no implementacion. **No tocas codigo.** No commitees. No arranques
el juego. Solo lees y escribes tu informe.

Tu plan es bueno si cumple las cuatro:

1. **Adjudica el solape.** Hay una auditoria de 2026-09-07 que **nadie ha leido ni cruzado**
   contra el trabajo ya triado. Decir cuanto de ella es nuevo es la mitad del valor de este
   encargo.
2. **Las lanes son disjuntas por FICHERO.** Dos lanes que tocan `LFPG_NetworkManagerImpl.c`
   no son paralelas: son un conflicto de merge con pasos extra.
3. **Cada lane trae su gate de aceptacion**, ejecutable o comprobable por lectura.
4. **Dice lo que NO haria y por que.** Un plan que mete los ~170 items es un inventario, no
   un plan.

## 3. FUENTES OBLIGATORIAS

**Todas las fuentes viven bajo `P:\LFPowerGrid\`**, que es un alias sin espacios, para que
las leas sea cual sea tu workspace. **Abrelas de verdad**: una cita sin `path:line` leido por
ti no cuenta, y este council ya ha visto lanes inventarse firmas de metodos con el arbol
delante.

Llamo `<RUN>` a `P:\LFPowerGrid\reviews\2026-09-08-council-plan-definitivo`.

| ruta | que es |
|---|---|
| `<RUN>\fuentes\AUDITORIA-2026-09-07.md` | **La auditoria sin leer.** 22 KB, 136 lineas. Copia fiel (sha256 `84d1392bf4150a3d…`) del original en `LFPowerGrid_dev`. **Prioridad 1.** |
| `P:\LFPowerGrid\reviews\2026-09-08-triaje-fichas-p2-p3\` | 5 informes `TRIAJE-*.md` con las 101 fichas P2/P3 triadas, cada una con veredicto y `path:line` |
| `P:\LFPowerGrid\reviews\2026-09-08-backlog-paralelo\` | Oleada 1: 5 lanes, sus `DICTAMEN-GROK.md` con los MEDIO/MENOR |
| `P:\LFPowerGrid\reviews\2026-09-08-backlog-paralelo-oleada2\` | Oleada 2. En `l7-debt-v3\INFORME-ASTRA.md` estan los 7 CONFLICTO |
| `P:\LFPowerGrid\reviews\2026-09-06-council-auditorias\PLAN-CONJUNTO.md` | El plan vigente, con sus tramos T0a..T6 |
| `P:\LFPowerGrid\AGENTS.md` | **Reglas del repo.** Linter obligatorio, orden de compilacion, convenciones de Enforce, trampa de persistencia |
| `<RUN>\fuentes\HANDOFF.md` | Estado vivo. Lee **solo** el primer bloque, hasta el primer `---` tras "Cola que queda" |
| `P:\LFPowerGrid\scripts\` y `P:\LFPowerGrid\config.cpp` | El arbol real |

**No leas los `PLAN.md` de `<RUN>\lanes\`.** Ahi escriben las otras lanes y romperias la
ceguera. Si te encuentras uno, no lo abras y dilo en tu seccion `G`.

**Mods hermanos:** ninguno. LFPowerGrid no comparte codigo con los otros mods del arbol; no
pierdas tiempo censandolos.

## 4. HECHOS VERIFICADOS

Medidos hoy por el orquestador, con su metodo. Puedes refutarlos, pero cita.

- **`main` = `e5b5303`**, arbol limpio, 0 sin empujar. 19 commits fusionados hoy.
- **Linter offline en verde.** `python C:/Users/guill/DayZ-Modding-Knowledge-Pack/tools/dayz-script-validator/scripts/script_validator.py .`
  sobre `main`: **263 ficheros, 0 errores, 47 warnings**, exit 2. Ojo: `status` vale `WARN`
  con cero errores y el exit 2 es aprobado; se gatea por `len(errors)`.
- **El mundo carga.** Arranque diag 2026-09-08 12:01, servidor y cliente: cero `Can't compile`,
  cero `Unknown type`, Mission 590 clases en servidor / 588 en cliente. Evidencia en
  `P:\LFPowerGrid\reviews\2026-09-08-arranque-verificacion\`.
- **Las fichas P2/P3: 101 = 92 VIVA + 6 DUDOSA + 3 MUERTA.** Universo: filas de las tablas de
  veredicto de los cinco `TRIAJE-*.md` (dispositivos 18, grafo 18, red/seguridad 16,
  render/UI 27, sorter 22). DUDOSAS: `D20 G16 R26 S10 S13 S18`. MUERTAS: `SEC03 R25 S22`.
- **La auditoria trae 46 hallazgos.** Universo: 34 numerados (`S1-S7` sobreingenieria,
  `H1-H9` servidor, `C1-C12` cliente, `U1-U6` unificacion) + 5 viñetas de su §5 assets/build
  + 7 propuestas de su §6.
- **9 MEDIO vivos y 11 MENOR** de los dictamenes de Grok. Universo: 8 MEDIO de oleada 1 menos
  1 ya arreglado en `d59cad8`, mas 2 de `l7`; MENOR = 7 de oleada 1 + 2 de `l6` + 2 de `l7`.
- **7 CONFLICTO** de `debt/v3-data-integrity`, todos en el camino del dinero. Universo: filas
  `CONFLICTO` de la tabla de `l7-debt-v3\INFORME-ASTRA.md`.
- **Tres ramas sin fusionar**: `archive/t2-autoridad-servidor` (aporta 1.162 inserciones netas
  en 4 ficheros de `5_Mission`, **nunca compilada ni arrancada**), `debt/v3-data-integrity` y
  `maint/audit-kimi-followup` (estas dos ya vaciadas por las lanes `l7` y `l8`).
- **La V3 del sorter esta jubilada de INTERFAZ, no de entidad.** Se borraron 5 clases de UI y
  3 layouts; `LFPG_Sorter` y `LFPG_Sorter_Kit` se conservan porque `LFPG_Sorter_TEST` **hereda**
  de ellos en script (`scripts\4_World\test\LFPG_Sorter_TEST.c`) y en `config.cpp`.
- **Hay jugadores reales en un servidor privado.** Los classnames de `CfgVehicles` estan
  ligados a persistencia.
- **`action_use` del MCP no completa acciones continuas** (las de barra de progreso). Eso deja
  sin probar desmontar y desplegar por automatismo.

## 5. INTERPRETACIONES DEL ORQUESTADOR

**Esto NO esta medido. Es donde mas facil me equivoco. Atacalo.**

- Creo que **una fraccion grande de los 46 hallazgos de la auditoria ya esta cubierta** por las
  92 fichas y los 9 MEDIO, y que el numero real de trabajo nuevo es bastante menor que 170.
- Creo que **la decision de producto que mas cambia el tamaño del plan es si el fork `_TEST`
  sale del release**. La auditoria le pone un 3/10 y propone sacarlo. Si sale, muchas fichas
  de `*_TEST.c` dejan de importar. Si se queda, hay que unificarlo con una base comun.
- Creo que **`LFPG_NetworkManagerImpl.c` (6.989 lineas) es el cuello de botella del
  paralelismo**, porque muchisimos items caen dentro.
- Creo que el orden que mas rinde es: cruzar la auditoria primero, luego decidir lo del `_TEST`,
  y solo entonces repartir. Puede que este mal y convenga empezar por otro lado.

## 6. LO PRIMERO QUE ESCRIBES: LA CRITICA DEL ENCUADRE

**Antes de proponer nada**, responde: *¿que puede estar mal en el encuadre de este encargo o en
las opciones que propone?* Puedes conservar un hecho y refutar la inferencia. Este council
aporta valor cuando puede rechazar la premisa, no solo elegir entre lo que heredo.

Precedente real que lo justifica: hace unas horas, un plan aprobado ordenaba borrar
`LFPG_Sorter.c`. Una lane se **nego** y tenia razon: habria roto la compilacion de World y las
bases de los jugadores. La negativa valia mas que la obediencia.

## 7. CONTRATO DE SALIDA

Escribe **un solo fichero**: `PLAN.md`, en el directorio de trabajo que te den. Estas
secciones, con estos titulos exactos y en este orden:

```
## A. CRITICA DEL ENCUADRE
## B. SOLAPE AUDITORIA <-> TRABAJO YA TRIADO
## C. LA DECISION DE PRODUCTO
## D. EL PLAN: LANES
## E. ORDEN Y PARALELISMO
## F. LO QUE NO HARIA
## G. LO_NO_VERIFICADO
```

Que va en cada una:

- **A.** Tu critica del §6. Si crees que el encargo esta bien planteado, dilo y explica por que.
- **B.** Una tabla: cada hallazgo de la auditoria (`S1`..`U6`, y las viñetas de §5/§6) contra
  la ficha, MEDIO o CONFLICTO que ya lo cubre — o `NUEVO`. Tres columnas:
  `hallazgo | cubierto_por | evidencia (path:line)`. **Es la seccion mas importante.** Si no
  te da tiempo a todo, prioriza esta.
- **C.** Tu veredicto sobre el fork `_TEST`, con su consecuencia **numerica**: cuantos items
  desaparecen o aparecen segun se decida. Si crees que la decision relevante es otra, dilo y
  argumenta cual.
- **D.** Las lanes. Por cada una: `id | objetivo | ficheros exclusivos que toca | items que
  cierra | gate de aceptacion | de que lane depende`. Los ficheros son **exclusivos**: si dos
  lanes tocan el mismo fichero, no son paralelas y hay que decir como se resuelve.
- **E.** Que puede correr a la vez, que bloquea a que, y **cuantas lanes simultaneas** aguanta
  el plan sin conflictos.
- **F.** Lo que dejarias fuera, con motivo. Coste/beneficio, riesgo sobre jugadores, o
  "esto no es codigo, es una decision del dueño".
- **G.** Todo lo que afirmaste sin poder verificarlo, y por que. **Una seccion `G` vacia es una
  señal de alarma, no de calidad.**

Reglas de cita: **cada API, clase, metodo o linea que nombres lleva `path:line` que abriste
tu**. Lo que no puedas verificar va a `G`, no al cuerpo del plan.

## 8. FRONTERAS

- **Solo lectura.** No edites, crees ni borres ningun fichero del repo. Tu unica escritura es
  tu `PLAN.md` en tu directorio de trabajo.
- No ejecutes `git` mas alla de lecturas (`log`, `show`, `diff`, `status`).
- No arranques el juego ni intentes construir el PBO.
- No propongas borrar ni renombrar un classname de `CfgVehicles` sin decir explicitamente que
  riesgo de persistencia corre.

## 9. PRESUPUESTO

Trabaja con esfuerzo alto pero acotado. Si tienes que elegir, el orden de valor es
**B > C > D > E > A > F > G**. Un `PLAN.md` con B y C solidos y D esbozado vale mas que uno
con las siete secciones a medias.
