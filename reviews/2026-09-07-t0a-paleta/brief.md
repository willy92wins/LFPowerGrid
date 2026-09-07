# BRIEF — T0a: extraer la paleta de color a un modulo neutro

## 1. La tarea, en una frase

Sacar la clase `LFPG_ColorData` y las **29 constantes `COL_*`** de dentro de `LFPG_SorterView.c` a
un modulo propio, y repuntar **solo el ATM** para que las consuma desde ahi.

## 2. Por que existe este encargo (contexto que cambia como lo haces)

`LFPG_SorterView.c` es la UI **V3 del sorter**, que esta condenada: hay una V4 (`_TEST`) que la
va a sustituir y la V3 se borrara. El problema es que el **ATM** —que no tiene nada que ver con
el sorter y no se va a borrar— consume su paleta. Mientras eso siga asi, **no se puede borrar la
V3 sin romper el ATM**.

Este encargo no arregla ningun bug. Su unico producto es **desacoplar**, para que el borrado de
la V3 sea posible mas adelante. Eso decide dos cosas:

- **La V3 tiene que seguir funcionando exactamente igual** al terminar. No es un paso previo a
  borrarla en esta corrida; es un paso previo a poder borrarla algun dia.
- **Cero cambios de comportamiento visual.** Si un color cambia un solo bit, has fallado. Esto es
  un movimiento mecanico, no un rediseño.

## 3. Rol y criterio de exito

**Rol: EJECUCION.** Produces un parche en el arbol de trabajo y un informe.

Exito, y es comprobable con grep:

    grep -rn "LFPG_SorterView\.COL_" scripts/4_World/LFPG_BTCAtmView.c scripts/4_World/LFPG_BTCAtmController.c

debe devolver **0 lineas** al terminar, y los 29 nombres de color deben seguir existiendo y valer
**exactamente lo mismo** que ahora.

## 4. Hechos verificados por el orquestador (2026-09-07, contra el arbol que tienes)

- `class LFPG_ColorData extends Managed` — `scripts/4_World/LFPG_SorterView.c:45`
- **29 constantes `COL_*` unicas**, y son estas exactamente:

      COL_AMBER COL_BG_DEEP COL_BG_ELEVATED COL_BG_INPUT COL_BG_PANEL COL_BG_RULES_PANEL
      COL_BG_SECTION COL_BG_SECTION_CARD COL_BLUE COL_BLUE_BTN COL_BTN COL_CATCHALL_BG
      COL_GREEN COL_GREEN_BORDER COL_GREEN_BTN COL_GREEN_DIM COL_HEADER COL_INPUT_BORDER
      COL_PAIRING_ERR COL_PAIRING_OK COL_PURPLE COL_RED COL_RED_BTN COL_RED_BTN_BORDER
      COL_RED_BTN_SOFT COL_SEPARATOR COL_TEXT COL_TEXT_DIM COL_TEXT_MID

- Consumidores del ATM: **69 lineas** con `COL_` en `scripts/4_World/LFPG_BTCAtmView.c` y **18**
  en `scripts/4_World/LFPG_BTCAtmController.c`. Ademas **5 lineas** con `LFPG_ColorData` en
  `LFPG_BTCAtmView.c`.
  *(Un plan anterior decia «76 referencias». Esa cifra es vieja, de otro commit. Manda tu recuento
  sobre el arbol que tienes: hazlo y ponlo en el informe.)*
- `LFPG_ColorData_TEST` existe, en `test/LFPG_SorterView_TEST.c`.

## 5. Alcance positivo

1. Crear un modulo neutro nuevo para la paleta. **Donde y como se llame es decision tuya**, pero
   tiene que cargarse antes que sus consumidores: mira el orden de modulos de Enforce
   (`3_Game` antes que `4_World` antes que `5_Mission`) y decide en consecuencia. Justifica la
   ubicacion en el informe.
2. Mover ahi `LFPG_ColorData` y las 29 constantes, **con los mismos valores**.
3. Repuntar `LFPG_BTCAtmView.c` y `LFPG_BTCAtmController.c` al modulo nuevo.
4. Dejar que `LFPG_SorterView.c` (la V3) consuma el modulo nuevo. **La V3 no se rompe hoy.**

## 6. Fronteras negativas — leelas dos veces, hay una trampa cara

**NO puedes:**

- **Unificar `LFPG_ColorData` con `LFPG_ColorData_TEST`.** Esta explicitamente fuera de alcance.
  Motivo: esa union toca `SetUserData`, donde una auditoria documenta un **crash de heap
  (`0xc0000374`)**. No es una extraccion mecanica y no entra hoy. Si te tienta, para y escribelo
  en `VISTO-DE-PASO`.
- **Cambiar ningun valor de color.** Ni «normalizar», ni «unificar duplicados», ni pasar de hex a
  constantes con nombre. Si dos constantes tienen el mismo valor, **siguen siendo dos**.
- Tocar `test/LFPG_SorterView_TEST.c` ni nada `_TEST`.
- Tocar `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` ni
  `scripts/5_Mission/LFPG_NetworkManagerImpl.c` — **hay otras lanes dentro de esos dos ficheros
  ahora mismo**.
- Tocar `config.cpp`, ni `scripts/3_Game/LFPG_BTCConfig.c`, `LFPG_FileUtil.c`,
  `scripts/5_Mission/LFPG_BTCHelper.c`, `LFPG_BalanceProvider_NativeImpl.c`.
- `git commit`, `git push`, `git checkout`, `git reset`. **Deja los cambios sin commitear.**

**Refactor no relacionado: NO.** Lo que veas y no sea la extraccion va a `VISTO-DE-PASO`.

## 7. Convenciones de la casa — obligatorias, el compilador de DayZ es estricto

Este repo **no tiene `AGENTS.md`**, asi que van aqui. No son estilo: romperlas rompe el build.

1. **Cero ternarios `? :`**. Usa `if`/`else`.
2. **Nada de `++` ni `--`**. Se escribe `i = i + 1`.
3. **Nada de `+=` ni `-=`**. Se escribe `x = x + y`.
4. **Nada de `foreach`**. `while` o `for` con indice explicito.
5. **`ref` solo en miembros de clase.** Nunca en parametros, retornos, locales ni typedefs.
   Ojo: `LFPG_ColorData` se usa con `ref array<ref LFPG_ColorData>` como **miembro**
   (`LFPG_SorterView.c:70`); eso es correcto y se queda.
6. **Log por `LFPG_Util`**: `.Error()`, `.Warn()`, `.Info()`, `.Debug()`. No uses `Print()`.
7. **TRAMPA MEDIDA:** dos secuencias de escape adyacentes en un literal revientan la compilacion
   del modulo `World` con `CParser: quoted string not closed`. Una llamada por linea, cero escapes.
8. Nombres: `m_` miembros, `s_` estaticos, PascalCase metodos, camelCase locales.
9. **Si creas un fichero `.c` nuevo, comprueba si hay que declararlo en algun sitio** para que el
   modulo lo compile. Miralo en el arbol; no lo supongas.

## 8. Permisos y prohibiciones

- Tienes shell y escritura **dentro de tu workspace**.
- **No salgas del workspace.** Nada de `P:\Mods`, `$profile:`, ni el arbol del juego.
- **No lances DayZ, ni AddonBuilder, ni ningun build de PBO.**

## 9. Gate

**Aviso, y es un limite de ESTE host, no tuyo:** una lane anterior no pudo ejecutar Python —
un hook de PowerShell mata su shell antes de arrancarlo. **Intentalo**:

    python enfcheck.py <cada fichero que toques>

Si el shell te lo tumba, **no pelees con el**: dilo en el informe y haz el equivalente con `grep`
(comillas impares, balance de `{}` y `()`, y las convenciones 7.1-7.4 sobre tus lineas nuevas).
El orquestador corre el linter de verdad al recibir. **No inventes una salida de linter que no
ejecutaste**: se compara.

Gate propio de esta ficha, barato y decisivo — **correlo tu y pega la salida**:

    grep -rn "LFPG_SorterView\.COL_" scripts/4_World/LFPG_BTCAtmView.c scripts/4_World/LFPG_BTCAtmController.c
    grep -c "COL_" scripts/4_World/LFPG_BTCAtmView.c scripts/4_World/LFPG_BTCAtmController.c

El primero debe salir vacio. El segundo debe dar **los mismos numeros que antes de empezar**
(69 y 18 segun mi medida — confirma tu la linea base ANTES de tocar nada). Si el recuento baja,
has perdido una referencia por el camino.

**Y el gate que de verdad importa, que es de datos:** haz una tabla de los 29 colores con su
valor **antes** y **despues**, leidos del codigo. Si alguno difiere, es un bug que has
introducido tu.

## 10. Limites del entorno que no puedes ver

- **Enforce no se compila aqui.** AddonBuilder empaqueta los `.c` pero **no los compila**: el
  juego compila al cargar. Nadie sabra si esto compila hasta que el orquestador arranque un
  servidor. El linter y las convenciones son lo unico que te protege — y en una extraccion como
  esta, el modo de fallo tipico es **orden de carga de modulos**, no sintaxis.
- Trabajas en un **git worktree aislado**, rama `feat/t0a-paleta`. Otras tres lanes trabajan a la
  vez en otras ramas sobre otros ficheros. Por eso el §6 es duro.
- Tope de reloj: **50 minutos**. Escribe `REPORT.md` por tramos segun avances, no al final.

## 11. Entregable

`REPORT.md` en la raiz del workspace, mas los cambios sin commitear.

    ## RECUENTO
    (tu linea base medida: cuantas referencias en cada fichero antes y despues)

    ## TABLA DE LOS 29 COLORES
    | constante | valor antes | valor despues | igual? |

    ## QUE HICE
    - **Donde puse el modulo y por que** (orden de carga)
    - **Que declare y donde**, si hizo falta declararlo
    - `path:line` de los puntos tocados

    ## QUE NO CUBRE
    (obligatorio; en particular: que queda todavia atando el ATM a la V3, si algo)

    ## SALIDA DE LOS GREPS DEL GATE
    (literal)

    ## LO QUE NO PUDE VERIFICAR

    ## LA PREMISA DE ESTE ENCARGO
    Casilla obligatoria: **que puede estar mal en el planteamiento?** En particular: es el
    modulo neutro la forma correcta de romper este acoplamiento, o hay una mas simple que no
    hemos visto? Hay algo en el ATM que dependa de la V3 **ademas** de la paleta, y que haga
    que esta extraccion no baste para el objetivo declarado? Dilo aqui.

    ## VISTO-DE-PASO

## 12. Quien te revisa

El orquestador (Anthropic) abre cada `path:line` que cites, repite los greps y el linter, y manda
tu parche a revision adversarial de **otra familia de modelos**. Una cita inventada invalida el
hallazgo: **no cites de memoria, abre el fichero**.
