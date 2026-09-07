# BRIEF — T2 lane B: SEC01, destinatarios de RPC en LFPG_NetworkManagerImpl

## 1. La tarea, en una frase

Cerrar la ficha P1 **SEC01**: las sincronizaciones que el mod trata como «unicast» o filtradas por
proximidad **se estan difundiendo a todos los clientes**, en `scripts/5_Mission/LFPG_NetworkManagerImpl.c`.

## 2. Rol y criterio de exito

**Rol: EJECUCION.** Produces un parche en el arbol de trabajo y un informe.

Exito = cada envio dirigido lo esta de verdad, sin cambiar a quien le llega la informacion que SI
debe ser global. Un `REPORT.md` con la tabla completa de los 9 sitios y **que NO cubre** tu
arreglo.

**Esto es un servidor de DayZ con jugadores reales dentro.** El riesgo aqui tiene dos caras y las
dos importan: de menos (un jugador deja de recibir un sync que necesitaba y ve cables fantasma) y
de mas (un jugador recibe el estado de la base de otro). **Ante la duda, no cambies el sitio y
documentalo** — un sync de mas es un bug de red; un sync de menos roto en produccion es un mod que
no funciona.

## 3. Carga inicial (rutas dentro de tu workspace)

1. `scripts/5_Mission/LFPG_NetworkManagerImpl.c` — 7.786 lineas. **No la leas entera**: ve a los
   9 sitios de la tabla del §4 y lee la funcion que contiene cada uno.
2. `P:\scripts\3_game\gameplay.c:104-126` si tienes acceso de lectura a `P:` — es la definicion
   vanilla de `ScriptRPC`. Si no lo tienes, el §4 te la cita textual.

## 4. El hecho verificado, y es corto

**La firma vanilla, verificada por el orquestador el 2026-09-07:**

    P:\scripts\3_game\gameplay.c:117
        proto native void Send(Object target, int rpc_type, bool guaranteed, PlayerIdentity recipient = NULL);

y el comentario de la propia BI, una linea encima:

    P:\scripts\3_game\gameplay.c:115
        @param recipient specified client to send RPC to. If NULL, RPC will be send to all
        clients (specifying recipient increase security and decrease network traffic)

**El cuarto argumento decide si el RPC va a UNO o a TODOS.** Vanilla dice explicitamente que
especificarlo *aumenta la seguridad*.

**Los 9 sitios de `.Send(` en tu fichero**, con el nombre de la variable que ocupa ese cuarto
parametro:

| linea | 4o argumento | dirigido? |
|---|---|---|
| 2151 | `noExclude` (= `null`) | **NO** |
| 2340 | `noExclude` (= `null`) | **NO** |
| 2484 | `null` literal | **NO** |
| 2618 | `noExclude` (= `null`) | **NO** |
| 2663 | `noExclude` (= `null`) | **NO** |
| 2783 | `noExclude` (= `null`) | **NO** |
| 2984 | `null` literal | **NO** |
| 3027 | `noExclude` (= `null`) | **NO** |
| **6361** | **`pid`** | **SI** |

**El sitio 6361 es tu control positivo**: demuestra que el patron correcto ya existe en este mismo
fichero. Copialo, no inventes.

    LFPG_NetworkManagerImpl.c:6361
        refreshRpc.Send(pb, LFPG_RPC_CHANNEL, true, pid);

**El caso mas claro, para que veas la forma del defecto.** En `:2140-2151` hay un bucle que filtra
por proximidad y acto seguido difunde a todo el mundo:

    LFPG_NetworkManagerImpl.c:2140
        if (!inRange) continue;
        ...
    LFPG_NetworkManagerImpl.c:2149-2151
        bool bRpcGuaranteed = true;
        PlayerIdentity noExclude = null;
        rpc.Send(pb, LFPG_RPC_CHANNEL, bRpcGuaranteed, noExclude);

El `continue` de proximidad **no filtra nada** a efectos de red: se calcula quien esta en rango y
luego se manda a todos igual, una vez por cada jugador en rango. El coste tambien se multiplica.

**Y el que la auditoria llamo peor:** `:2783` envia sobre `m_FullSyncPlayer` — o sea que el codigo
ya SABE a que jugador va dirigido — y aun asi pasa `null`.

## 5. Lo que hay que conseguir, y la trampa que tiene

**Lo obvio:** que cada sitio cuyo emisor ya conoce el destinatario lo pase.

**La trampa, y es la razon de que esto sea una ficha y no un `sed`:** el nombre de la variable
`noExclude` sugiere que alguien penso que ese parametro era una *exclusion* («no excluyas a
nadie») y no un *destinatario*. Si esa lectura equivocada se propago, puede haber sitios donde el
comportamiento actual —difundir— sea el que el resto del sistema espera, y dirigirlo rompa a un
cliente que hoy recibe ese RPC de rebote.

**Por eso el encargo NO es «cambia los 8 a dirigido».** Es, sitio por sitio:

1. Determina **quien deberia recibir esto** leyendo la funcion que lo contiene: hay un
   `PlayerIdentity` o un `PlayerBase` a mano? el bucle ya filtra por proximidad o por propiedad?
   el nombre del RPC (`SYNC_OWNER_WIRES_V2`, etc.) implica destinatario?
2. Si el destinatario es **inequivoco**, dirigelo.
3. Si es **ambiguo o parece deliberadamente global**, **dejalo como esta** y explica por que en la
   tabla del informe.

Un sitio dejado a proposito, razonado, cuenta como trabajo hecho. Un sitio cambiado sin razon es
un bug nuevo en un servidor con jugadores.

**Ojo con `PlayerIdentity` nulo.** Comprueba si en el contexto de cada sitio la identidad puede
ser null (jugador desconectando, entidad sin dueño). Pasar un null ahi te devuelve el
comportamiento de difusion sin avisar, asi que si lo diriges, gestiona ese caso explicitamente.

## 6. Alcance positivo y fronteras negativas

**PUEDES tocar:**
- `scripts/5_Mission/LFPG_NetworkManagerImpl.c`

**NO puedes tocar, por ningun motivo:**
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` — **otra lane esta dentro ahora mismo**
- `config.cpp`
- `scripts/3_Game/LFPG_BTCConfig.c`, `LFPG_FileUtil.c`, `scripts/5_Mission/LFPG_BTCHelper.c`,
  `LFPG_BalanceProvider_NativeImpl.c` — tramo T1, recien commiteado
- nada del sorter (`LFPG_SorterView*`, `LFPG_MCP_*`, `gui/`)
- `git commit`, `git push`, `git checkout`, `git reset`. **Deja los cambios sin commitear.**

**Refactor no relacionado: NO.** Lo que veas y no sea SEC01 va a `VISTO-DE-PASO`.

## 7. Convenciones de la casa — obligatorias, el compilador de DayZ es estricto

Este repo **no tiene `AGENTS.md`**, asi que van aqui. No son estilo: romperlas rompe el build.

1. **Cero operadores ternarios `? :`**. Usa `if`/`else`.
2. **Nada de `++` ni `--`**. Se escribe `i = i + 1`.
3. **Nada de `+=` ni `-=`**. Se escribe `x = x + y`.
4. **Nada de `foreach`**. Bucles `while` o `for` con indice explicito.
5. **`ref` solo en miembros de clase.** Nunca en parametros, retornos, locales ni typedefs.
6. **Log por `LFPG_Util`**: `.Error()` (nivel 0, siempre se escribe), `.Warn()`, `.Info()`,
   `.Debug()`. No uses `Print()`.
7. **TRAMPA MEDIDA:** dos secuencias de escape adyacentes en un literal revientan la compilacion
   del modulo `World` con `CParser: quoted string not closed`. Una llamada por linea, cero escapes.
8. Nombres: `m_` miembros, `s_` estaticos, PascalCase metodos, camelCase locales.
9. Concatenacion de strings y argumentos: en una sola linea.

## 8. Permisos, riesgo y prohibiciones

- Tienes shell y escritura **dentro de tu workspace**. Usalo.
- **No salgas del workspace.** Nada de `P:\Mods`, `$profile:`, ni el arbol del juego.
- **No lances DayZ, ni AddonBuilder, ni ningun build de PBO.**
- No toques credenciales ni red.

## 9. El gate que TIENES que correr tu

En tu workspace hay `enfcheck.py`. Correlo **antes de tu primer edit** para tener linea base, y
otra vez al terminar. Pega las dos salidas en el informe:

    python enfcheck.py scripts/5_Mission/LFPG_NetworkManagerImpl.c

Comprueba comillas impares, balance de llaves y parentesis, escapes adyacentes y las convenciones
7.1-7.4. **No es un compilador**: que pase no prueba que compile, pero que falle prueba que no.

Gate propio de esta ficha, y es barato: al terminar, `grep -n "\.Send(" ` sobre el fichero y
comprueba que el recuento de sitios sigue siendo **9**. Si aparecen o desaparecen envios, algo se
te fue de las manos.

## 10. Limites del entorno que no puedes ver (te los damos porque no son adivinables)

- **Enforce no se compila aqui.** AddonBuilder empaqueta los `.c` pero **no los compila**: el
  juego compila al cargar. Nadie sabra si tu codigo compila hasta que el orquestador arranque un
  servidor. El linter y las convenciones son lo unico que te protege.
- **No hay hot-reload.**
- Trabajas en un **git worktree aislado**, rama `fix/t2-sec01-recipients`. Otra lane trabaja en
  otra rama sobre `LFPG_RPCServerHandlerImpl.c`. Por eso la prohibicion del §6 es dura.
- **El trafico real no esta medido.** La auditoria dice que la ganancia de red «se debe medir» y
  nadie la ha medido. **No prometas cifras de ahorro**: di que sitios diriges y por que, no cuanto
  ancho de banda ahorras.
- Tope de reloj de esta corrida: **50 minutos**. Escribe `REPORT.md` por tramos segun avances, no
  al final.

## 11. Entregable

Un unico fichero `REPORT.md` en la raiz de tu workspace, mas los cambios en el `.c` sin commitear.

Empieza por **la tabla de los 9 sitios**, con una fila por sitio:

    | linea | funcion | RPC | destinatario deducido | accion | por que |

donde `accion` es `DIRIGIDO`, `SE QUEDA GLOBAL` o `AMBIGUO — NO TOCADO`.

Despues, la ficha:

    ### SEC01 — CERRADA | PARCIAL | NO CERRADA
    - **Que cambie:** con `path:line`
    - **Por que asi:** y que alternativa descartaste
    - **Que NO cubre:** el residuo vivo. Obligatorio
    - **Como comprobarlo:** el experimento concreto in-game

Y al final, cuatro secciones fijas:

    ## SALIDA DEL LINTER
    (literal, antes y despues)

    ## LO QUE NO PUDE VERIFICAR

    ## LA PREMISA DE ESTE ENCARGO
    Casilla obligatoria: **que puede estar mal en el planteamiento que te hemos dado?**
    En particular: crees que la lectura «`noExclude` es un destinatario mal entendido» es
    correcta? Hay algun sitio donde difundir sea claramente lo que el diseño queria y el
    orquestador te lo haya presentado como defecto? Dilo aqui. Un "todo correcto" en las otras
    secciones acota solo lo que te hemos preguntado, no lo que el trabajo necesitaba.

    ## VISTO-DE-PASO

## 12. Quien te revisa

El orquestador (Anthropic) verifica cada `path:line` que cites abriendolo, repite el linter y el
recuento de `.Send(`, y manda tu parche a una **revision adversarial de otra familia de modelos**.
Una cita plausible pero inventada invalida el hallazgo entero: **no cites de memoria, abre el
fichero**.
