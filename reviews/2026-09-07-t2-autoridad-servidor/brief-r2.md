# BRIEF — T2 ronda 2: el reemplazo de cable como transaccion

## 0. Lee esto primero: el fallo de la ronda 1 fue del encuadre, no de quien la escribio

La ronda 1 de T2 la hicieron **dos** agentes en paralelo. Yo, el orquestador, la parti **por
ficheros**: una lane en `LFPG_RPCServerHandlerImpl.c`, la otra en `LFPG_NetworkManagerImpl.c`, y a
cada una le **prohibi** tocar el fichero de la otra para que no chocaran al fusionar.

Un revisor de otra familia la rechazo. Su frase, que es la que manda en este brief:

> Los cinco focos sirven, pero los riesgos determinantes aparecen **entre** ellos: admision del
> almacen frente a admision del grafo, y destinatarios nuevos frente a clientes que conservan el
> estado anterior. **La separacion por archivo ocultaba ambas relaciones.**

Y sobre mi veto en concreto:

> La lane A explica honestamente que no puede hacer rollback por su reparto de ficheros, pero eso
> no permite cerrar la ficha como si la propiedad de producto estuviera satisfecha. **El limite de
> una lane no cambia el comportamiento exigido al sistema.**

Tiene razon. El arreglo correcto vive **a caballo entre los dos ficheros**, y yo hice imposible
escribirlo. **Ese veto queda levantado: en esta ronda eres dueño de los dos.** No defiendas el
codigo de la ronda 1 ni lo trates como intocable — buena parte se salva, pero la estructura del
reemplazo hay que rehacerla.

## 1. La tarea, en una frase

Convertir el reemplazo de cable en una **transaccion**: tiene exito solo si lo admiten el almacen
**y** el grafo, y solo entonces se publica a los clientes.

## 2. Rol y criterio de exito

**Rol: EJECUCION.** Produces un parche en el arbol de trabajo y un informe.

Exito = F-01 y F-02 cerrados, F-03 cerrado o razonado, **sin romper lo que ya esta bien**, y un
`REPORT.md` que diga por cada hallazgo que NO cubre tu arreglo.

**Hay jugadores reales en un servidor privado.** Ante la duda entre permisivo y restrictivo,
restrictivo (regla G6: los handlers de RPC fallan cerrados). Pero ojo: **un rechazo falso tambien
es un fallo** — F-02 es exactamente eso.

## 3. Carga inicial

En la raiz de tu workspace:

1. `REVIEW-CODEX.md` — **el dictamen completo. Leelo entero antes de tocar nada.** Trae los
   escenarios de fallo paso a paso, las citas `path:line` del mecanismo y un anexo con los modelos
   que ejecuto. Es tu especificacion.
2. `DECISIONES-PRODUCTO.md` — la decision de producto **ya firmada por el dueño** sobre SEC02.
   Es requisito, no opcion.
3. `INFORME-RONDA1-laneA.md` y `INFORME-RONDA1-laneB.md` — lo que hizo cada lane y, sobre todo,
   sus secciones `Que NO cubre`.
4. `git diff` — las 366 lineas de la ronda 1, ya aplicadas a tu arbol.

## 4. Los tres hallazgos que tienes que cerrar

Los resumo; **el detalle y los escenarios estan en `REVIEW-CODEX.md`** y son mas precisos que este
resumen.

### F-01 [GRAVE] — el rechazo del grafo deja el almacen mintiendo

El reemplazo borra el incoming anterior y guarda el nuevo **antes** de intentar insertarlo en el
grafo. Si el grafo lo rechaza (tope de aristas por nodo, `LFPG_MAX_EDGES_PER_NODE = 12` en
`LFPG_Defines.c:520`), la ronda 1 hace rebuild y `return` sin revertir nada: el cable nuevo **se
queda en el almacen**, el anterior ya no esta, y el rebuild lo vuelve a rechazar.

Y lo que tumba la mitad (b) de SEC20: no difundir *ese* add no evita el fantasma, porque el
**full sync posterior serializa el almacen entero** sin preguntar al grafo. El revisor lo localiza
en `SendVanillaWiresTo` (`LFPG_NetworkManagerImpl.c:2644-2649`), llamado desde `:2827-2835`.

**Es residuo de SEC20/SEC03, no una regresion nueva.** Pero sigue vivo y es el GRAVE.

### F-02 [MEDIO] — falso «device is full», y es regresion nueva

El preflight de admision que introdujo la ronda 1 resta del recuento **solo** los cables del mismo
puerto de origen. La fase real de reemplazo tambien quita los que apuntan al **puerto de destino**,
incluidos los que salen de otro puerto del mismo origen. El helper ni siquiera recibe el destino,
asi que no puede contarlos.

Escenario del revisor: `MaxWiresPerDevice=1` (valido: el minimo es 1,
`LFPG_Settings.c:212-213`), splitter S con `output_1..output_3` (`LFPG_Splitter.c:51-60`), spotlight
D. Mover `S.output_1 -> D` a `S.output_2 -> D` se rechaza, cuando antes cabia y el total final
seguia siendo 1.

**Esto es la replica de una regla desincronizandose de la regla real.** Que el arreglo no vuelva a
replicarla es parte del encargo.

### F-03 [MEDIO] — el unicast hace efectivo un filtro de interes incompleto

El broadcast vanilla calcula los interesados con el owner y los destinos que **quedan despues** de
mutar. Al quitar o redirigir un cable, quien observaba el destino **viejo** se cae de la lista.
Antes le llegaba por rebote de la difusion a `null`; ahora ya no, y se queda con el cable pintado.

**Y aqui la ronda 1 se equivoco en un numero, ojo con heredarlo:** el informe de la lane B justifica
que da igual porque «el renderer ya no dibujaba a mas de 50 m». Falso. El descarte por owner usa
`earlyOutDist = LFPG_CULL_DISTANCE_M + 25.0` = **75 m**
(`LFPG_CableRenderer.c:2385-2387` y `:2417-2441`), mas una burbuja de 25 m en los extremos. Entre
70 y 75 m el cliente **dibuja y no recibe**.

El revisor apunta el contraste util: el camino de **deltas nativos ya conserva los destinos de las
operaciones eliminadas** (`LFPG_NetworkManagerImpl.c:2435-2444`). Ahi tienes el contrato ya escrito
en el arbol.

## 5. Lo que NO puedes romper

Esto ya esta bien y el revisor lo dice explicitamente. Si tu refactor lo tumba, has fallado:

- **SEC02.** *«esta bien resuelta respecto a la decision firmada»*. El reemplazo respeta
  `AllowCutOthersWires` y, al chocar con un cable ajeno, **aborta la operacion entera**. Es
  decision del dueño, firmada, y **no se reabre**. Lee `DECISIONES-PRODUCTO.md`.
- **SEC20(a).** La validacion del nombre de puerto para dispositivos vanilla contra
  `GetPortCount`/`GetPortName`/`GetPortDir`. Se queda.
- **SEC01.** Los destinatarios explicitos se quedan. F-03 **no** se arregla volviendo a `null`:
  eso reintroduce la ficha. Se arregla ampliando a quien se invalida.

## 6. Alcance positivo y fronteras negativas

**Eres dueño de los dos ficheros:**
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`
- `scripts/5_Mission/LFPG_NetworkManagerImpl.c`

Puedes crear ficheros nuevos bajo `scripts/` si el diseño lo pide (dilo en el informe).

**NO puedes tocar, y esta vez el motivo es real, no un veto mio de reparto:**
- `scripts/3_Game/LFPG_FileUtil.c`, `scripts/5_Mission/LFPG_BTCHelper.c`,
  `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`, `scripts/3_Game/LFPG_BTCConfig.c`,
  `scripts/5_Mission/LFPG_MissionInit.c` — **hay otra entrega sin fusionar sobre esos cinco**
  (instrumento de inyeccion de fallos). Tocarlos cuesta una fusion manual.
- `scripts/4_World/LFPG_SorterView.c`, `LFPG_BTCAtmView.c`, `LFPG_BTCAtmController.c`,
  `LFPG_SorterController.c`, `LFPG_SorterPreviewRow.c`, `LFPG_SorterTagView.c` — **otra entrega sin
  fusionar** (extraccion de paleta).
- `config.cpp` — classnames ligados a persistencia.
- `git commit`, `git push`, `git checkout`, `git reset`. **Deja los cambios sin commitear.**

**Refactor no relacionado: NO.** Lo que veas y no sea F-01/F-02/F-03 va a `VISTO-DE-PASO`.

## 7. El contrato que se te pide, y donde acaba tu libertad

**Lo exigido (no negociable):** un reemplazo tiene exito solo si lo admiten almacen **y** grafo, y
se publica a los clientes **despues** de esa decision. Si falla cualquiera de los dos, el estado
observable —almacen, indice inverso, grafo y lo que ven los clientes— queda **como estaba antes de
intentarlo**.

**Como lo consigues es tuyo.** No te impongo el mecanismo. Piensa, entre otras cosas: si conviene
calcular el conjunto completo de cables que se van a quitar **antes** de quitar ninguno; si el
rollback necesita guardar las filas retiradas para reponerlas; si la operacion entera pertenece al
manager en vez de a una replica parcial de sus reglas en el handler (el revisor opina que si, y
ahora tienes los dos ficheros para moverla).

**Una nota sobre el formato en disco:** el revisor dice *«mantendria el formato persistente
existente»* y *«los datos anteriores requieren una politica explicita de validacion o reparacion;
no los borraria silenciosamente al actualizar»*. Si tu diseño necesita cambiar el formato guardado
o purgar filas viejas, **no lo hagas: escribelo en `REPORT.md` como propuesta** y sigue con el
resto.

## 8. Convenciones de la casa — obligatorias, el compilador de DayZ es estricto

Este repo **no tiene `AGENTS.md`**, asi que van aqui. No son estilo: romperlas rompe el build.

1. **Cero ternarios `? :`**. Usa `if`/`else`.
2. **Nada de `++` ni `--`**. Se escribe `i = i + 1`.
3. **Nada de `+=` ni `-=`**. Se escribe `x = x + y`.
4. **Nada de `foreach`**. `while` o `for` con indice explicito.
5. **`ref` solo en miembros de clase.** Nunca en parametros, retornos, locales ni typedefs.
6. **Log por `LFPG_Util`**: `.Error()` (nivel 0, siempre se escribe), `.Warn()`, `.Info()`,
   `.Debug()`. No uses `Print()`.
7. **TRAMPA MEDIDA:** dos secuencias de escape adyacentes en un literal revientan la compilacion
   del modulo `World` con `CParser: quoted string not closed`. Una llamada por linea, cero escapes.
8. Nombres: `m_` miembros, `s_` estaticos, PascalCase metodos, camelCase locales.
9. Si creas un `.c` nuevo: los modulos se declaran **por carpeta** en `config.cpp:215-228`
   (`files[] = { "LFPowerGrid/scripts/3_Game" }` y hermanos), asi que un fichero nuevo en una
   carpeta ya declarada se compila solo. **No hace falta tocar `config.cpp`.**

## 9. Permisos, riesgo y prohibiciones

- Tienes shell y escritura **dentro de tu workspace**.
- **No salgas del workspace.** Nada de `P:\Mods`, `$profile:`, ni el arbol del juego.
- **No lances DayZ, ni AddonBuilder, ni ningun build de PBO.**

## 10. Gate

**Limite de ESTE host, no tuyo, y ya mordio a las cuatro lanes de la ronda 1:** el shell no arranca
Python — un hook de PowerShell muere con `syntax error near unexpected token '&'` antes de lanzar
el proceso. **Intentalo igual**:

    python enfcheck.py scripts/5_Mission/LFPG_RPCServerHandlerImpl.c
    python enfcheck.py scripts/5_Mission/LFPG_NetworkManagerImpl.c

Si te lo tumba, **no pelees**: dilo y haz el equivalente con la herramienta de busqueda de tu
arnes. **No inventes una salida de linter que no ejecutaste**: yo la repito y se compara.

Dato util para no dar un falso positivo: en `LFPG_RPCServerHandlerImpl.c` el balance de parentesis
del linter da **`delta=-2` ya en la linea base**, por parentesis dentro de comentarios. Lo que
importa es que **no empeore**, no que sea cero. Mide la linea base antes de tu primer edit.

Gates propios de esta ronda, correlos y pega la salida:

- `grep -c "Send(.*, null)"` sobre los dos ficheros → debe seguir siendo **0**.
- El recuento de `.Send(` en `LFPG_NetworkManagerImpl.c` → **9**, salvo que tu diseño añada
  invalidaciones nuevas; si cambia, **justifica cada envio nuevo**.

## 11. Limites del entorno que no puedes ver

- **Enforce no se compila aqui.** AddonBuilder empaqueta los `.c` pero **no los compila**: el juego
  compila al cargar. Nadie sabra si esto compila hasta que yo arranque un servidor. El linter y las
  convenciones son lo unico que te protege.
- **No hay hot-reload.**
- Trabajas en un **git worktree aislado**, rama `fix/t2-r2-transaccion`, con las 366 lineas de la
  ronda 1 ya aplicadas. Hay otras dos entregas sin fusionar sobre otros ficheros; de ahi el §6.
- El mod corre en **muchos** servidores distintos. Un arreglo que dependa de una configuracion
  concreta es fragil — y `MaxWiresPerDevice` va de 1 a 128.
- Tope de reloj: **60 minutos**. Si no llegas a los tres, prioriza **F-01, luego F-02, luego
  F-03**. Escribe `REPORT.md` por tramos segun avances, no al final.

## 12. Entregable

`REPORT.md` en la raiz, mas los cambios sin commitear. Una seccion por hallazgo:

    ### F-01 | F-02 | F-03 — CERRADO | PARCIAL | NO CERRADO
    - **Que cambie:** con `path:line`
    - **Por que asi:** y que alternativa descartaste
    - **Que NO cubre:** el residuo vivo. Obligatorio
    - **Como comprobarlo:** el experimento concreto in-game

Mas estas secciones fijas:

    ## LO QUE NO ROMPI
    Como demuestras que SEC01, SEC02 y SEC20(a) siguen en pie tras tu refactor.
    Esta seccion es tan importante como las tres de arriba.

    ## SALIDA DEL LINTER Y DE LOS GATES
    (literal, antes y despues)

    ## LO QUE NO PUDE VERIFICAR

    ## LA PREMISA DE ESTE ENCARGO
    Casilla obligatoria. **Que puede estar mal en el planteamiento?** En particular:
    - Es «transaccion almacen+grafo» el contrato correcto, o el revisor y yo nos hemos
      equivocado de frontera?
    - Hay algun caso donde revertir sea PEOR que dejar el estado a medias?
    - Te he vuelto a vetar algo que hacia falta para cerrar un hallazgo? La ronda 1 se rechazo
      exactamente por eso, asi que **dilo sin miramientos**.

    ## VISTO-DE-PASO

## 13. Quien te revisa

Yo abro cada `path:line` que cites, repito el linter y los gates, y tu parche vuelve a **revision
adversarial de la misma familia que rechazo la ronda 1** — que ya conoce este codigo y va a buscar
si has cerrado sus tres hallazgos o solo los has movido de sitio. Una cita plausible pero inventada
invalida el hallazgo que sostiene: **no cites de memoria, abre el fichero**.
