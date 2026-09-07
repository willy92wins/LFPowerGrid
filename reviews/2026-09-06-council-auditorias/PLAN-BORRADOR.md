# Plan conjunto de acción — BORRADOR DEL ORQUESTADOR (para ser atacado)

Este borrador NO es la conclusión. Es la posición del orquestador, puesta por escrito para que
cada lane pueda refutarla. Si el encuadre está mal, decirlo es más valioso que ordenar bien un
menú equivocado.

## A. Hechos verificados en esta sesión (con su medición)

| # | Hecho | Cómo se midió |
|---|---|---|
| H1 | La auditoría grande apunta a `d61705e`, que **es el merge-base exacto de las cuatro ramas vivas**. No está desfasada: `main` sigue en `d61705e` y lleva 17 commits de retraso respecto al tronco real `289592b` | `git merge-base`, `git rev-list --count` |
| H2 | De las 151 fichas, **34 viven en ficheros que ningún commit ha tocado** desde `d61705e`; 117 en ficheros que sí cambiaron | pre-filtro: intersección de ficheros citados por ficha contra `git diff --name-only --ignore-all-space d61705e..HEAD` |
| H3 | Triage de las 25 fichas P1: **0 CORREGIDAS**. 11 VIVO + 1 PARCIAL verificadas leyendo código; 6 vivas por H2; 7 en curso | lectura dirigida con `path:line` del árbol actual |
| H4 | Los 22 commits desde `d61705e` fueron sorter V4, split fachada/impl, privacidad de logs y deuda v3. **Ninguno atacó integridad** — la auditoría se entregó el 05-sep, después de casi todos | `git log d61705e..HEAD` |
| H5 | Huella del sorter: V3 = 143.010 B / **7 clases**; V4 TEST = 178.279 B / **10 clases**. V4 es 35 kB MÁS GRANDE que V3. Árbol entero: 286 clases, 2.818.739 B | `grep -c 'class'`, `stat` |
| H6 | **Recortar líneas NO libera arena.** Medición propia del proyecto: 6.466 líneas fuera = 1 kB; 1.071.376 B de fuente fuera = −2/0 kB. Solo la estructura compilada rindió: 15 clases = 44/45 kB | `assumptions.md` 2026-07-25/28, citado en `HANDOFF.md:159-161` |
| H7 | Sin rastro de despliegue público: no hay Workshop ID ni `publisherId` en `config.cpp`, ni mención de servidor en producción en el handoff | grep sobre `config.cpp` y `HANDOFF.md` |

**Advertencia de instrumento (H2):** «fichero intacto» es evidencia fuerte de que la ficha sigue
viva; «fichero tocado» **no** es evidencia de que se arreglara. El pre-filtro solo ahorra lectura
en la primera dirección. Los 217 clases que cuento hoy en `4_World` no casan con el 199→185 que
registró la palanca A2: mi regex y su método de conteo pueden no ser el mismo, así que la ratio
kB/clase se usa como orden de magnitud, no como cifra exacta.

## B. La consecuencia incómoda de H5 + H6

El objetivo declarado del proyecto es **jubilar la V3 para dejar más hueco en el mod**. Por la
medición propia del proyecto, eso rinde:

- borrar 143.010 B de fuente V3 → **~0 kB de arena** (H6);
- borrar **7 clases** → ~21 kB de arena por la ratio A2 (3,0 kB/clase).

Es decir: **la jubilación de la V3 vale ~21 kB de arena, no 143 kB.** Y hoy el mod carga las dos
implementaciones a la vez, así que el estado actual es el peor de los tres posibles.

## C. La decisión de producto que este council debe cambiar

Las dos auditorías dan órdenes **incompatibles** y ninguna de las dos conoce el objetivo del
proyecto:

- La auditoría grande (§8) ordena 12 PRs con **integridad primero** (PR1 destinatarios RPC, PR2
  permisos/puertos, PR3 commit/abort monetario) y pone **«Consolidación UI TEST/normal» en la
  PR 11 de 12**.
- La auditoría muse y el trabajo en vuelo ponen la **consolidación primero**.
- Chocan de verdad: las PR6/PR7 de la grande arreglan contrato y scheduler del sorter — código
  que la jubilación de la V3 reescribe o borra. Arreglar bugs en código que vas a borrar es
  trabajo tirado; borrar antes de arreglar puede llevarse por delante correcciones que la V4 no
  tiene (la ficha **S10** ya dice que las dos UIs **divergen** en protección de preview).

**La pregunta:** ¿se cierra y jubila la V3 ANTES o DESPUÉS de los P1 de integridad?

## D. Posición del orquestador (atacadla)

**Orden propuesto: T0 → T1 → T2 → T3 → T4, con T5 empezando en paralelo desde el día 1.**

| Tramo | Contenido | Por qué ahí |
|---|---|---|
| **T0** | Cerrar V4 y jubilar V3: decidir la paridad `powered/linked` de la acción TEST, cerrar los bloqueadores del censo, borrar las 7 clases V3, renombrar `_TEST` → nombres definitivos | Es lo único en vuelo; quita el impuesto de escribir cada arreglo del sorter dos veces; S10 dice que el fork YA diverge. **Alcance duro: sin refactor.** |
| **T1** | Integridad monetaria: E04, E16, SEC09 residual, E02, E03, E08 | Es lo único que destruye valor del jugador de forma irreversible |
| **T2** | Autoridad de servidor: SEC02, SEC20, SEC01 | Saltos de política y destinatarios de red equivocados |
| **T3** | Dispositivos rotos: D04 (clase PAS que no existe en config), D05 (adaptador irrecuperable), D01/D02/D03 | Funciones muertas o que mienten al jugador |
| **T4** | Grafo y coste: G01, G02, G04, G18, R02, R04, S04, S08 | Ninguno pierde datos; son trabajo evitable y coherencia |
| **T5** | Instrumento: A03 + línea base del §7 de la auditoría | A03 dice que la cobertura actual valida el checker, **no** la lógica. Sin esto, ningún tramo se puede declarar sin regresión |

**Razonamiento de por qué T0 va primero, pese a B:** no porque la huella lo merezca —no lo
merece, son 21 kB— sino porque el fork es un **impuesto recurrente** sobre todo lo que venga
después, y T1..T4 incluyen 24 fichas del sorter (S01–S24) que habría que escribir dos veces
mientras vivan las dos UIs.

**Razonamiento de por qué T1 no va primero:** H7. Sin despliegue público, la pérdida de dinero es
**latente**, no daño activo. Si ese hecho es falso —si hay servidor con jugadores— **el orden se
invierte y T1 pasa a ser lo primero**.

## E. Lo que el orquestador NO ha verificado

- Las 117 fichas en ficheros tocados, salvo las P1: **P2 y P3 están sin triar**. El plan las trata
  como clases, no como fichas individuales.
- Si `Fence.OpenFence()` respeta la política de candado del motor (decide si D16 es exploit real).
- Si existe servidor privado con jugadores que H7 no detecta.
- La ratio kB/clase de A2 aplicada a la V3 es una proyección, no una medida sobre este borrado.

## F. Lo que el censo V3 cambió del propio borrador (añadido tras medirlo)

El borrador daba T0 por «pequeño y en vuelo». **Es falso.** El censo de acoplamientos, verificado
por grep propio, dice que jubilar la V3 no es borrar cuatro ficheros:

- `LFPG_ColorData` se define en `LFPG_SorterView.c:45`, y allí viven **29 constantes `COL_*`**.
- El **cajero BTC depende de esa paleta con 76 referencias**: `LFPG_BTCAtmView.c` (64 `COL_*` + 5
  `LFPG_ColorData`) y `LFPG_BTCAtmController.c` (12 `COL_*`). *Borrar `LFPG_SorterView.c` tumba el
  ATM.*
- `LFPG_MissionInit.c` engancha la V3 en nueve sitios (precreación, ESC, muerte/inconsciencia,
  cleanup, y el mutex anti dual-open) y `LFPG_RPCClientHandler.c` en cuatro (Open, SaveAck,
  SortAck, PreviewData).
- El propio `LFPG_SorterView_TEST.c` referencia la V3 **13 veces** (entre ellas el mutex de
  `Open`, `:1329`).
- `LFPG_SorterController`, `LFPG_SorterTagView` y `LFPG_SorterPreviewRow` **no tienen clientes
  externos**: esos tres sí se borran limpio.

**Consecuencia que el borrador no vio:** T0 y T1 tocan **el mismo subsistema**. Extraer la paleta
obliga a reescribir 76 líneas del cajero, y el cajero es justo donde viven E04/E16/SEC09. Hacerlos
por separado significa tocar el ATM dos veces, con la segunda vez encima de una refactorización
recién hecha y sin cobertura (A03).

**Revisión de la posición del orquestador:**

| Tramo | Cambio respecto a D |
|---|---|
| **T0a (nuevo, primero)** | **Extracción pura de la paleta**: sacar `LFPG_ColorData` y las 29 `COL_*` de `LFPG_SorterView.c` a un fichero neutro (p. ej. `3_Game/LFPG_UIPalette.c`) y repuntar los 76 usos del ATM + los 13 de V4. **Cero cambio de comportamiento**, commit aislado, revisable por diff mecánico. Desbloquea T0 y desacopla el ATM del sorter para siempre |
| **T0b** | Lo que era T0: paridad `powered/linked`, `MissionInit` + `RPCClientHandler`, `IsOpen()` de TEST en `LFPG_Actions.c:528` y `LFPG_ActionSyncSorter.c:69`, emisor de `SORTER_TEST_RESYNC (65)` y productor de `SORTER_TEST_CARGO_REFRESH (70)`, y borrado de las 7 clases V3 |
| **T1** | Sin cambio, pero ahora **se beneficia** de que T0a ya desacopló el ATM |

**Y una advertencia que el borrador no daba:** la ficha **S08** dice que el repack síncrono sin
presupuesto **lo hereda la V4** (`RPCServerHandlerImpl.c:147-150` enruta `SORTER_TEST_REQUEST_SORT`
al mismo handler). Jubilar la V3 **no** arregla S04/S08: los arrastra.
