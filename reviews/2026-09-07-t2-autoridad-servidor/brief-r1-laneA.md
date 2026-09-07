# BRIEF — T2 lane A: autoridad de servidor en HandleFinishWiring

## 1. La tarea, en una frase

Cerrar tres fichas P1 de seguridad (SEC02, SEC03, SEC20) que viven **todas dentro de la misma
funcion** `LFPG_RPCServerHandlerImpl.HandleFinishWiring`, en `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`.

## 2. Rol y criterio de exito

**Rol: EJECUCION.** Produces un parche en el arbol de trabajo y un informe.

Exito = las tres fichas cerradas o razonadas como no-cerrables, el fichero compilando segun el
linter estructural que te damos, y un `REPORT.md` que diga por cada ficha **que NO cubre** tu
arreglo. Un informe sin seccion de limites se considera incompleto.

**Esto es un servidor de DayZ con jugadores reales dentro.** Un fallo aqui no es deuda tecnica:
es un jugador cortando cables de la base de otro. Ante la duda entre permisivo y restrictivo,
**restrictivo** (regla G6 de la casa: los handlers de RPC fallan cerrados por defecto).

## 3. Carga inicial (rutas dentro de tu workspace)

1. `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c` — la funcion `HandleFinishWiring` va de la
   linea 222 a la 791. Leela ENTERA antes de tocar nada: son 570 lineas y las tres fichas
   interactuan.
2. `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`, funcion `HandleCutWires` (`:793` en adelante).
   **Es tu referencia de como se hace bien**: ahi la politica SI se aplica.
3. `scripts/5_Mission/LFPG_NetworkManagerImpl.c` — solo para leer las firmas de
   `RemoveWiresTargeting`, `CountWiresTargeting`, `NotifyGraphWireAdded`, `BroadcastOwnerWireDelta`
   y `BroadcastVanillaWires`. **No lo edites: otra lane esta trabajando en ese fichero.**

## 4. Las tres fichas, con lo ya verificado

Todo lo de esta seccion esta **verificado por el orquestador** contra el arbol que tienes delante
(2026-09-07). Los numeros de linea son de tu copia. Si algo no casa, **para y dilo**: significa
que el arbol se movio.

### SEC02 — el reemplazo de cable salta la politica `AllowCutOthersWires`

**Hecho verificado.** En `HandleFinishWiring`, la fase de reemplazo llama:

    LFPG_RPCServerHandlerImpl.c:680
        int removedIn = LFPG_NetworkManager.Get().RemoveWiresTargeting(dstRealId, dstPort);

Dos argumentos. En cambio el camino de CORTE, en `HandleCutWires`, llama la misma funcion asi:

    LFPG_RPCServerHandlerImpl.c:954
        int inRemoved = LFPG_NetworkManager.Get().RemoveWiresTargeting(deviceId, inPort, cutPid, allowOthers);
    LFPG_RPCServerHandlerImpl.c:1799
        int removed = LFPG_NetworkManager.Get().RemoveWiresTargeting(deviceId, portName, cutPid, allowOthers);

donde `allowOthers` sale de la configuracion (`:837-839`):

        bool allowOthers = false;
        ...
            allowOthers = st.AllowCutOthersWires;

**Consecuencia:** un jugador que no puede CORTAR el cable de otro si puede borrarselo
conectando el suyo al mismo puerto de entrada. La politica solo cubre una de las dos puertas.

**Lo que hay que conseguir:** que la fase de reemplazo respete `AllowCutOthersWires` igual que el
corte. **El COMO es tuyo.** Piensa si el rechazo debe ocurrir *antes* de empezar a mutar el grafo
(nota que la fase de reemplazo ya empezo a borrar cables de origen antes de llegar al :680).

**Decision de producto que NO puedes tomar tu:** si al rechazar hay que abortar toda la operacion
o solo saltarse ese borrado concreto. Si tu lectura del codigo no la resuelve de forma obvia,
implementa la variante **restrictiva** (abortar), dejalo escrito en `REPORT.md` bajo
`DECISION-DE-PRODUCTO` y sigue.

### SEC03 — el reemplazo borra aunque lo nuevo no se pueda almacenar

**Hecho verificado.** El orden actual es: borrar los cables en conflicto (`:600-684`), y solo
DESPUES intentar guardar el nuevo (`:690-702`):

    LFPG_RPCServerHandlerImpl.c:691
        stored = LFPG_DeviceAPI.AddDeviceWire(srcObj, wd);
    LFPG_RPCServerHandlerImpl.c:702
        stored = LFPG_NetworkManager.Get().AddVanillaWire(srcRealId, wd);

Si `stored` sale false, el bloque `:704-741` **ya no puede deshacer los borrados**: cierra la
mutacion, suelta el lock, persiste la eliminacion a proposito (`:713-719`, comentario
«server restart would resurrect the removed wire») y fuerza un rebuild (`:730-736`). El jugador
recibe *"Wire already exists or device is full."* con sus cables anteriores ya destruidos.

**Lo que hay que conseguir:** que un fallo de admision no destruya conexiones existentes.

**Pista de diseño, no receta:** la causa de que `stored` falle (duplicado, o dispositivo lleno)
es en principio **decidible antes** de borrar nada. Un preflight que responda «esto se va a poder
guardar» convierte el problema en un rechazo limpio. Si encuentras un caso en que no sea
decidible por adelantado, dilo en `REPORT.md` en vez de forzarlo.

**No inventes una API nueva en `LFPG_NetworkManager` si puedes evitarlo:** ese fichero lo esta
tocando otra lane y un choque de firmas nos cuesta la fusion.

### SEC20 — el servidor no valida el nombre de puerto de un dispositivo vanilla

**Hecho verificado, y es la mas grave de las tres.** La unica comprobacion del nombre de puerto
que se aplica siempre es de LONGITUD:

    LFPG_RPCServerHandlerImpl.c:283-287
        if (srcPort.Length() > 32 || dstPort.Length() > 32)
        {
            LFPG_Util.Warn("[FinishWiring-Server] denied (port too long)");
            return;
        }

La validacion de verdad esta detras de dos banderas:

    LFPG_RPCServerHandlerImpl.c:465-466
        bool srcIsLFPG = (LFPG_DeviceAPI.GetDeviceId(srcObj) != "");
        bool dstIsLFPG = (LFPG_DeviceAPI.GetDeviceId(dstObj) != "");

y `HasPort` (`:469`, `:478`) y `CanConnectTo` (`:489`) solo corren si esas banderas son true.
**En un dispositivo vanilla `GetDeviceId()` devuelve `""`**, asi que las dos son false y un
cliente modificado puede mandar cualquier cadena de <=32 caracteres como nombre de puerto: se
almacena y entra en el indice inverso sin que nadie compruebe que ese puerto existe.

**Segundo hecho verificado, el de la divergencia store/grafo.** El orden al final de la funcion es:

    :748-758   BroadcastOwnerWireDelta / BroadcastVanillaWires   <-- difunde a los clientes
    :765       bool edgeAdded = ... NotifyGraphWireAdded(...)    <-- inserta en el grafo
    :783-790   if (!edgeAdded) -> Warn + PostBulkRebuildAndPropagate()

Se **difunde antes de insertar**, y cuando la insercion falla no se revierte el store ni se
desdifunde: se fuerza un rebuild. Store y grafo quedan divergentes en la ventana intermedia.

**Lo que hay que conseguir:** dos cosas separables — (a) que el nombre de puerto se valide en
servidor tambien para dispositivos vanilla, y (b) que no se difunda un cable que todavia puede no
entrar en el grafo. **Si solo te da tiempo a una, haz (a)**: es la que abre la puerta.

**Limite del entorno que tu no puedes ver:** los dispositivos vanilla de DayZ tienen **un solo
puerto** y el grafo impone 12 aristas por direccion. La auditoria dejo escrito que el escenario de
`MaxWiresPerDevice=128` es **distinto** de este y que confundirlos genera un falso positivo. No
mezcles los dos.

## 5. Alcance positivo y fronteras negativas

**PUEDES tocar:**
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`
- crear ficheros nuevos bajo `scripts/` si tu diseño lo pide (dilo en el informe)

**NO puedes tocar, por ningun motivo:**
- `scripts/5_Mission/LFPG_NetworkManagerImpl.c` — **otra lane esta dentro ahora mismo**
- `config.cpp` — classnames ligados a persistencia; romperlos rompe mundos guardados
- `scripts/3_Game/LFPG_BTCConfig.c`, `LFPG_FileUtil.c`, `scripts/5_Mission/LFPG_BTCHelper.c`,
  `LFPG_BalanceProvider_NativeImpl.c` — acaban de commitearse (tramo T1), fuera de alcance
- nada del sorter (`LFPG_SorterView*`, `LFPG_MCP_*`, `gui/`)
- `git commit`, `git push`, `git checkout`, `git reset`. **Deja los cambios sin commitear.**

**Refactor no relacionado: NO.** Si ves algo feo que no es SEC02/03/20, anotalo en `REPORT.md`
bajo `VISTO-DE-PASO` y no lo toques.

## 6. Convenciones de la casa — obligatorias, el compilador de DayZ es estricto

Este repo **no tiene `AGENTS.md`**, asi que van aqui. No son estilo: romperlas rompe el build.

1. **Cero operadores ternarios `? :`**. Usa `if`/`else`.
2. **Nada de `++` ni `--`**. Se escribe `i = i + 1`.
3. **Nada de `+=` ni `-=`**. Se escribe `x = x + y`.
4. **Nada de `foreach`**. Bucles `while` o `for` con indice explicito.
5. **`ref` solo en miembros de clase.** Nunca en parametros, retornos, locales ni typedefs.
6. **Log por `LFPG_Util`**: `.Error()` (nivel 0, siempre se escribe), `.Warn()`, `.Info()`,
   `.Debug()`. No uses `Print()`.
7. **TRAMPA MEDIDA, y cuesta un build entero:** dos secuencias de escape adyacentes en un literal
   revientan la compilacion del modulo `World` con `CParser: quoted string not closed`. Si tienes
   que emitir texto con escapes, **una llamada por linea y cero escapes**.
8. Nombres: `m_` miembros de instancia, `s_` estaticos, PascalCase metodos, camelCase locales.
9. Concatenacion de strings y argumentos: en una sola linea.

## 7. Permisos, riesgo y prohibiciones

- Tienes shell y escritura **dentro de tu workspace**. Usalo.
- **No salgas del workspace.** Nada de `P:\Mods`, `$profile:`, ni el arbol del juego.
- **No lances DayZ, ni AddonBuilder, ni ningun build de PBO.** No estan disponibles para ti y el
  gate de compilacion real lo corre el orquestador.
- No toques credenciales ni red.

## 8. El gate que TIENES que correr tu

En tu workspace hay `enfcheck.py`. Correlo sobre el fichero que edites y **pega su salida en el
informe**:

    python enfcheck.py scripts/5_Mission/LFPG_RPCServerHandlerImpl.c

Comprueba comillas impares, balance de llaves y parentesis, escapes adyacentes y las convenciones
6.1-6.4. **No es un compilador**: que pase no prueba que compile, pero que falle prueba que no.
Si tu cambio sube cualquiera de esos contadores respecto al estado inicial, arreglalo antes de
entregar. Mide el estado inicial ANTES de tu primer edit para tener con que comparar.

## 9. Limites del entorno que no puedes ver (te los damos porque no son adivinables)

- **Enforce no se compila aqui.** AddonBuilder empaqueta los `.c` pero **no los compila**: el
  juego compila al cargar. Nadie sabra si tu codigo compila hasta que el orquestador arranque un
  servidor. Por eso el linter estructural y la disciplina de convenciones son lo unico que te
  protege.
- **No hay hot-reload.**
- Trabajas en un **git worktree aislado**, rama `fix/t2-finishwiring`. Otra lane trabaja en otra
  rama sobre otro fichero. Por eso la prohibicion del §5 es dura.
- Tope de reloj de esta corrida: **50 minutos**. Si ves que no llegas a las tres fichas, **cierra
  con lo que tengas y escribe el informe**. Un informe honesto de dos fichas vale mas que tres a
  medias sin documentar. Escribe `REPORT.md` por tramos segun avances, no al final.

## 10. Entregable

Un unico fichero `REPORT.md` en la raiz de tu workspace, mas los cambios en el `.c` sin commitear.

Estructura obligatoria de `REPORT.md`, una seccion por ficha (SEC02, SEC03, SEC20):

    ### <FICHA> — CERRADA | PARCIAL | NO CERRADA
    - **Que cambie:** con `path:line` de cada punto tocado
    - **Por que asi:** y que alternativa descartaste
    - **Que NO cubre:** el residuo que queda vivo. Obligatorio. "Nada" solo si lo puedes defender
    - **Como comprobarlo:** el experimento concreto que lo probaria in-game

Y al final, tres secciones fijas:

    ## SALIDA DEL LINTER
    (pegada literal, antes y despues)

    ## LO QUE NO PUDE VERIFICAR
    (todo lo que afirmaste sin poder comprobarlo aqui)

    ## LA PREMISA DE ESTE ENCARGO
    Casilla obligatoria: **que puede estar mal en el planteamiento que te hemos dado?**
    Si crees que una de las tres fichas esta mal diagnosticada, que el arreglo correcto vive en
    otro sitio, o que el orquestador te ha vetado algo que hacia falta para cerrarla — **dilo
    aqui**. Esta seccion no es cortesia: un "todo correcto" en las otras acota solo lo que te
    hemos preguntado, no lo que el trabajo necesitaba.

    ## VISTO-DE-PASO
    (lo que no tocaste y merece ficha)

## 11. Quien te revisa

El orquestador (Anthropic) verifica cada `path:line` que cites abriendolo, repite el linter, y
manda tu parche a una **revision adversarial de otra familia de modelos**. Una cita plausible pero
inventada invalida el hallazgo entero, asi que **no cites de memoria: abre el fichero**.
