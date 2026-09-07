# BRIEF — T5: instrumento de inyeccion de fallos para integridad monetaria

## 1. La tarea, en una frase

Construir el **instrumento** que permite provocar a voluntad los fallos que el tramo T1 dice
haber cerrado, para poder comprobarlos en un servidor real en vez de creerselos.

## 2. Lee esto antes que nada: que es y que NO es este encargo

**Este encargo produce un INSTRUMENTO, no resultados.**

Hay un tramo de codigo (T1, integridad monetaria) recien commiteado y verificado a medias: se
sabe que compila y que una de sus siete fichas funciona in-game. Las otras seis estan cerradas
«por construccion» — nadie ha provocado nunca los fallos que dicen manejar. El plan del proyecto
le puso a T1 este criterio de fin, literal:

> *matriz de fallos **con inyeccion** y reinicio entre cada par de E/S. Sin harness no se declara.*

Ese harness no existe. Tu lo construyes.

**Lo que NO haces, y es importante que no lo intentes:**

- **No arrancas DayZ.** No esta a tu alcance y las corridas in-game las hace el orquestador con un
  puente MCP que tu no tienes.
- **No produces resultados de pruebas.** Si tu informe contiene una tabla de casos «PASA/FALLA»,
  has fallado el encargo: seria inventado. Tu entregas la matriz **vacia**, lista para rellenar.
- **No declaras ninguna ficha cerrada.** No es tu decision y no tienes evidencia para tomarla.

**El unico modo de fallo catastrofico de esta lane es fabricar evidencia.** Un instrumento a
medias y honesto vale; una tabla de resultados verdes inventada envenena un tramo de codigo que
maneja el dinero de jugadores reales.

## 3. Rol y criterio de exito

**Rol: EJECUCION.** Produces codigo y documentacion en el arbol de trabajo.

Exito = el orquestador puede coger tu entrega, arrancar un servidor, provocar un fallo concreto de
la lista, reiniciar, y leer del log si el invariante aguanto — **sin escribir codigo nuevo**.

## 4. De donde sale la lista de casos (no te la inventes: ya esta escrita)

En tu workspace hay `reviews/2026-09-06-council-auditorias/IMPL-T1-INFORME.md`. Es el informe de
implementacion de T1, ficha por ficha. **Cada ficha tiene una seccion `Como comprobarlo` con el
experimento concreto ya redactado**, y otra `Que NO cubre` con el residuo vivo.

Ejemplos literales de ese fichero, para que veas la forma:

> **E04:** *«Sell a cuenta con inyeccion de fallo en `AtomicSaveBalances` tras el staging de
> spill: BTC y EUR de spill conservados, error INVALID, cero credito, sin marcador. Crash tras
> `AddBalance` OK y antes de destruir: al reconectar, credito intacto y BTC destruidos.»*

> **E03:** *«moneda valor 1 no apilable, amount que exija 65 entidades -> `AMOUNT_TOO_LARGE`,
> cero spawn (salvo probe). Limite 64 justo pasa; 65 falla.»*

Esa es tu especificacion. **Extrae de ahi la matriz completa** — las siete fichas (E02, E03, E04,
E05, E08, E16, SEC09) mas las secciones de segunda y tercera pasada al final del fichero.

Lee tambien `reviews/2026-09-06-council-auditorias/REVIEW-T1.md`: es la revision adversarial, y sus
hallazgos GRAVE describen los escenarios que mas importa poder reproducir.

## 5. El problema de diseño, que es el trabajo de verdad

Los casos se agrupan en dos familias, y **necesitan mecanismos distintos**:

**(a) Fallo devuelto.** Una funcion que normalmente tiene exito devuelve fallo: `AtomicSaveBalances`
false, `CopyFile` false, `DeleteFile` false, `AddBalance` parcial. Se provoca haciendo que esa
llamada mienta bajo demanda.

**(b) Muerte del proceso.** El servidor se cae *entre* dos operaciones — por ejemplo entre
`AddBalance` y `DestroyPlayerItems`, que es exactamente la ventana que la ficha E04 abrio a
proposito. No se puede simular devolviendo un error: el proceso tiene que dejar de existir ahi.

**Como resolver cada una es tuyo.** Piensa, entre otras cosas: como se nombra un punto de
inyeccion, como se arma desde fuera sin recompilar (hay un JSON de settings en `$profile:` que el
mod ya lee), como se arma **una sola vez** para que no se dispare en cada operacion, y como deja
constancia en el log de que se disparo —porque el log es lo unico que el orquestador podra leer
despues.

## 6. La restriccion que manda sobre el diseño

**Esto no puede llegar a un servidor de produccion. Hay jugadores reales.**

Un instrumento que puede provocar la perdida de dinero **tiene que ser imposible de activar por
accidente**. Ese es el requisito duro, y como lo consigues es tuyo. Dos ideas del arbol, por si
sirven: el mod ya usa un patron de gate de diagnostico (`LFPG_PERFDIAG_ENABLED`), y ya distingue
builds. Elige, justificalo, y **di explicitamente en el informe que pasa si alguien despliega tu
codigo tal cual en produccion**. Si la respuesta no es «nada», no has terminado.

Nota de contexto que refuerza esto: una auditoria encontro una sonda de diagnostico
(`S1_PROBE`) que **no** estaba detras de su gate y escupia ~16 lineas al log en cada apertura de un
panel. El fallo tipico aqui es exactamente ese: creer que algo esta gateado y que no lo este.

## 7. Alcance positivo

1. El **mecanismo de inyeccion** en Enforce.
2. Los **puntos de inyeccion** en el codigo de T1. Puedes editar esos ficheros **solo para eso**.
3. El **driver** que el orquestador correra desde fuera: lee el log del servidor, decide si el
   invariante aguanto. Lenguaje tuyo — hay Python en el entorno del orquestador.
4. La **matriz de casos vacia**, un caso por fila, con: ficha, punto de inyeccion, que se hace
   antes, que se espera despues del reinicio, y como se lee la respuesta del log.
5. Un **README de uso**: la secuencia exacta que el orquestador tiene que ejecutar.

## 8. Fronteras negativas

- **En los ficheros de T1 solo añades ganchos de inyeccion.** Nada de arreglar, mejorar ni
  reordenar su logica: acaba de verificarse en caja y no se toca. Los ficheros son
  `scripts/3_Game/LFPG_BTCConfig.c`, `scripts/3_Game/LFPG_FileUtil.c`,
  `scripts/5_Mission/LFPG_BTCHelper.c`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`,
  `scripts/5_Mission/LFPG_MissionInit.c`.
  **Con la inyeccion desarmada, el comportamiento tiene que ser identico al actual.** Esa es la
  propiedad que el orquestador va a comprobar leyendo tu diff entero.
- **NO toques** `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`,
  `scripts/5_Mission/LFPG_NetworkManagerImpl.c`, `scripts/4_World/LFPG_SorterView.c`,
  `LFPG_BTCAtmView.c`, `LFPG_BTCAtmController.c` — **hay tres lanes mas trabajando en esos
  ficheros ahora mismo**.
- **No toques** `config.cpp`.
- **No lances DayZ, ni AddonBuilder, ni ningun build de PBO.**
- `git commit`, `git push`, `git checkout`, `git reset`. **Deja los cambios sin commitear.**

## 9. Convenciones de la casa — obligatorias, el compilador de DayZ es estricto

Este repo **no tiene `AGENTS.md`**, asi que van aqui. No son estilo: romperlas rompe el build.

1. **Cero ternarios `? :`**. Usa `if`/`else`.
2. **Nada de `++` ni `--`**. Se escribe `i = i + 1`.
3. **Nada de `+=` ni `-=`**. Se escribe `x = x + y`.
4. **Nada de `foreach`**. `while` o `for` con indice explicito.
5. **`ref` solo en miembros de clase.** Nunca en parametros, retornos, locales ni typedefs.
6. **Log por `LFPG_Util`**: `.Error()` es nivel 0 y **se escribe siempre** — es el que quieres
   para las trazas de inyeccion, porque `.Info()` es nivel 1 y puede estar apagado.
7. **TRAMPA MEDIDA, y esta te va a morder porque vas a emitir mucho texto:** dos secuencias de
   escape adyacentes en un literal revientan la compilacion del modulo `World` con
   `CParser: quoted string not closed`. **Una llamada `Error()` por linea y cero escapes.** Asi
   esta escrito `LogCatalogHelp()` en `LFPG_BTCConfig.c`; copia ese patron.
8. Nombres: `m_` miembros, `s_` estaticos, PascalCase metodos, camelCase locales.
9. Si creas un `.c` nuevo, comprueba si hay que declararlo en algun sitio para que el modulo lo
   compile. Miralo en el arbol; no lo supongas.

## 10. Limites del entorno que no puedes ver

- **Enforce no se compila aqui.** AddonBuilder empaqueta los `.c` pero **no los compila**: el
  juego compila al cargar. El linter y las convenciones son lo unico que te protege.
- **No hay hot-reload.** `LFPG_BTCConfig.Load()` corre **una sola vez**, desde
  `LFPG_NetworkManagerImpl.c:534`. Si tu mecanismo de armado se lee de un fichero, se lee al
  arrancar y no despues. Eso condiciona el diseño: cada caso de la matriz implica un reinicio, y
  **eso ya estaba asumido** («reinicio entre cada par de E/S»).
- **El log es el unico canal de vuelta.** El orquestador leera el script log del servidor. Nada de
  lo que escribas a otro sitio le llegara facil.
- Trabajas en un **git worktree aislado**, rama `feat/t5-harness`. Tres lanes mas trabajan a la
  vez sobre otros ficheros. Por eso el §8 es duro.
- Tope de reloj: **50 minutos**. Si no llegas a todo, prioriza en este orden: **(1) la matriz de
  casos extraida del informe, (2) el mecanismo de inyeccion, (3) los ganchos, (4) el driver.** La
  matriz sola ya vale: es el trabajo de lectura que nadie ha hecho. Escribe `REPORT.md` por tramos
  segun avances, no al final.

## 11. Gate

**Aviso, y es un limite de ESTE host, no tuyo:** una lane anterior no pudo ejecutar Python — un
hook de PowerShell mata su shell antes de arrancarlo. **Intentalo**:

    python enfcheck.py <cada .c que toques>

Si el shell te lo tumba, **no pelees**: dilo en el informe y haz el equivalente con `grep`. El
orquestador corre el linter de verdad al recibir. **No inventes una salida de linter que no
ejecutaste**: se compara.

Gate propio de esta lane, y es el importante: **enseña que con la inyeccion desarmada no cambia
nada**. Recorre tu propio diff sobre los cinco ficheros de T1 y, gancho por gancho, justifica por
que la ruta normal es identica a la de antes. Si algun gancho no lo puedes defender, quitalo.

## 12. Entregable

`REPORT.md` en la raiz del workspace, mas el codigo, mas la matriz, sin commitear.

    ## MATRIZ DE CASOS (VACIA — sin resultados)
    | # | ficha | punto de inyeccion | preparacion | accion | reinicio? | invariante esperado | como se lee en el log |

    ## MECANISMO DE INYECCION
    - **Como se arma y se desarma**, y por que no puede activarse por accidente
    - **Que pasa si esto se despliega tal cual en produccion** — respuesta explicita
    - **Fallo devuelto vs muerte del proceso**: como resuelves cada familia

    ## GANCHOS
    | ficha | fichero:linea | que intercepta | por que ahi |

    ## PRUEBA DE NO-REGRESION
    (gancho por gancho: por que con la inyeccion desarmada el comportamiento es identico)

    ## DRIVER
    - Como se invoca, que lee, que decide

    ## COMO LO USA EL ORQUESTADOR
    (la secuencia literal, paso a paso)

    ## QUE NO CUBRE
    (obligatorio: que casos del informe de T1 NO son inyectables con tu mecanismo, y por que)

    ## LO QUE NO PUDE VERIFICAR
    (aqui va, obligatoriamente, que no has ejecutado ni un solo caso)

    ## LA PREMISA DE ESTE ENCARGO
    Casilla obligatoria: **que puede estar mal en el planteamiento?** En particular: es la
    inyeccion en codigo la forma correcta de comprobar estas siete fichas, o hay casos donde una
    prueba mas barata daria la misma certeza? Hay alguna ficha cuyo `Como comprobarlo` del informe
    de T1 sea, en tu lectura, **insuficiente para demostrar lo que dice demostrar**? Eso ultimo es
    lo mas valioso que puedes entregar: dilo aqui.

    ## VISTO-DE-PASO

## 13. Quien te revisa

El orquestador (Anthropic) abre cada `path:line`, corre el linter, **lee tu diff entero sobre los
cinco ficheros de T1 buscando cambios de comportamiento**, y manda la entrega a revision
adversarial de **otra familia de modelos**. Una cita inventada invalida el hallazgo: **no cites de
memoria, abre el fichero**.
