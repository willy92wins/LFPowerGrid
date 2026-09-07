# BRIEF — T2 ronda 3: cerrar el bucle. **Esta es la ultima.**

## 1. El VERDE, fijado ANTES de que empieces

Las dos rondas anteriores se cerraron sin criterio de exito definido, y las dos las rechazo un
revisor que siempre podia encontrar un escenario mas. **Eso se acaba aqui.** Este es el liston
completo, y no se va a mover mientras trabajas:

| # | Condicion | Como se comprueba |
|---|---|---|
| V1 | **N-01 cerrado** | Tras un intento rechazado, el recuento de nodos del grafo vuelve a ser el de antes del intento, y una operacion de cableado independiente y admisible **sigue pasando** |
| V2 | **N-02 cerrado** | Tras un lote de retiradas, ningun dispositivo queda **activo sin cable**. Control del revisor: el aspersor B acaba inactivo |
| V3 | **N-03 cerrado** | No sobrevive ninguna entrada en el mapa de invalidacion de un owner que no puede publicarse. Envio, diferimiento **y cancelacion** terminan el registro |
| V4 | **Residuo de F-01 cerrado** | Reponer las filas conserva el orden del almacen que decide la admision en el siguiente rebuild |
| V5 | **Residuo de F-03 cerrado** | `HandleCutPort` registra el extremo antiguo. El fallback de incoming, tambien |
| V6 | **Cero GRAVE nuevo** | — |
| V7 | **No se rompe lo que ya funciona** | F-02, SEC01, SEC02 y SEC20(a) siguen en pie, con evidencia |

**Si alcanzas V1-V7, T2 se cierra.** Si no los alcanzas, **no hay ronda 4**: el tramo se archiva
con lo que tenga y los residuos se anotan como deuda conocida. Asi que si ves que un punto no cabe
en el tiempo, **dilo y cierra bien los otros** — un V5 sin hacer y declarado vale mas que los siete
a medias.

## 2. Contexto: dos rondas, y lo que cada una enseño

- **Ronda 1**: dos lanes en paralelo, una por fichero. Rechazada. El revisor encontro que los
  riesgos vivian **entre** los ficheros, y mi veto impedia arreglarlos.
- **Ronda 2** (la tuya, este arbol): una lane con los dos ficheros. Introdujo la transaccion
  `TryCommitFinishWiring`. **Cerro F-02 del todo.** Rechazada otra vez: F-01 y F-03 quedaron
  parciales y aparecieron tres regresiones nuevas.

**Y el revisor se corrigio a si mismo, que es la clave para entender esta ronda:**

> Mi recomendacion anterior era insuficiente si se interpretaba como volver a meter filas y
> aristas: **el orden del store decide un rebuild; una insercion rechazada puede haber creado
> nodos; y las notificaciones modifican bombas/aspersores antes del commit.** Los limites de esa
> operacion deben incluir esos efectos y su restauracion, o posponerlos hasta disponer del estado
> comprometido.

O sea: la transaccion que escribiste esta **bien planteada** —el revisor confirma la frontera— pero
cubre menos efectos de los que la operacion produce. No la tires. **Ampliala.**

Tambien descarta explicitamente que juntar los ficheros fuera el error: *«Juntar los dos ficheros
en una lane permite resolver el contrato y no es causa de los defectos encontrados»*.

## 3. Las cinco cosas que hay que cerrar

El detalle completo, con escenarios paso a paso y citas, esta en **`REREVIEW-CODEX.md`** en esta
raiz. Leelo entero antes de tocar nada. Resumen para orientarte:

### N-01 [GRAVE] — un rechazo deja un nodo huerfano y puede bloquear el cableado del servidor

`AddEdgeInternal` puede **crear nodos** y despues rechazar la arista. El rollback des-inserta la
fila y repone conflictos, pero la limpieza diferida del batch solo purga extremos anotados al
**retirar** aristas, no los nodos creados por el intento. Al acumularse hasta el tope global de
2.048 nodos, **cualquier** cableado posterior falla.

**Por que es nueva:** el camino de rechazo anterior hacia `PostBulkRebuildAndPropagate`, que
republicaba el fantasma (eso era F-01) **pero de paso podaba el huerfano**. Tu ronda quito el
rebuild —correcto para F-01— sin sustituir la poda.

**Hipotesis del orquestador, NO una orden — verificala o descartala:** el motivo por el que se
quito ese rebuild era que reconstruia **sobre una fila rechazada que seguia en el almacen**. Tras
tu rollback esa fila **ya no esta**. Reconstruir *despues* de restaurar podria podar el huerfano
sin reintroducir F-01. El revisor desaconseja «reconstruir sobre filas rechazadas», que es cierto
para el caso de la ronda 1; despues del rollback la premisa cambio. **Si al mirarlo ves que no se
sostiene, dilo y resuelvelo a tu manera** — su alternativa es registrar y revertir los nodos
creados, con sus indices y contadores.

### N-02 [MEDIO] — el lote de notificaciones reactiva un aspersor cuyo cable se elimina

Agrupar **todas** las notificaciones de retirada antes de eliminar las filas cambia una propiedad
de la que dependia el codigo viejo: al notificar el segundo cable, el store todavia contiene el
primero, y eso **reactiva** el dispositivo del primero. Acaba activo y sin cable.

El revisor lo ejecuto en modelo: con el orden anterior B acaba inactivo; con el nuevo, activo.

### N-03 [MEDIO] — las invalidaciones de owners muertos se retienen sin limite

El mapa de posiciones de invalidacion se limpia **solo** en el camino de envio. Si el owner se
destruye antes del flush, o la cola descarta un owner nulo, o se registran posiciones para un owner
que no puede publicarse, la entrada se queda. Acumula durante la mision.

**Y una correccion a tu propio informe:** escribiste que las invalidaciones «viven hasta el envio
real». El revisor lo desmiente: el mapa se borra **antes** del bucle de destinatarios, asi que sin
ningun destinatario elegible se consume sin llegar a enviar. Ese orden por si solo no es otro bug,
pero la frase era falsa.

### F-01 residual [GRAVE] — reponer al final cambia que cable sobrevive a un rebuild

`FinishTxnRestoreConflicts` repone las filas **al final** del array. El orden del almacen es el que
decide que aristas se admiten en el siguiente rebuild, asi que un cable viejo puede desaparecer del
grafo despues, aunque su fila este de vuelta. Restaurar tiene que devolver la fila **a su sitio**,
no solo al array — para eso guardaste `m_Index` en `LFPG_FinishWiringRemovedWire`.

### F-03 residual [MEDIO] — el corte por puerto no registra el extremo antiguo

`HandleCutPort` elimina sin registrar la posicion del destino que desaparece, y el fallback de
incoming tampoco. El reemplazo y el corte completo si lo hacen. Falta cerrar esos dos productores.

## 4. La forma de pensarlo que da el revisor, y que vale mas que los cinco puntos sueltos

Nombra **tres comprobaciones** que faltaban, y las cinco cosas de arriba son sus consecuencias:

1. **rollback → orden del proximo rebuild** (F-01 residual)
2. **notificacion de retirada → estado de dispositivos** (N-02)
3. **cada productor de invalidacion → envio *o cancelacion*** (N-03, F-03 residual)

Y el principio general: *«las notificaciones del grafo no deben leer un store transitorio como
definitivo. Calcular el estado final sobre los cables comprometidos»*.

**Como lo implementas es tuyo.** No te impongo mecanismo. Pero si tu diseño hace que un consumidor
lea el almacen a media mutacion, vuelves a estar en N-02.

## 5. Lo que NO puedes romper (es V7, y se comprueba)

- **F-02.** El conjunto unico de conflictos que alimenta admision y mutacion. El revisor lo dio por
  cerrado y probo dos controles favorables. **No lo toques salvo que un arreglo lo exija**, y si lo
  exige, demuestra que sigue cerrado.
- **SEC02**, con la decision de producto firmada por el dueño (`DECISIONES-PRODUCTO.md`): al chocar
  con cable ajeno, aborta la operacion entera. **No se reabre.**
- **SEC20(a)**, la validacion de puerto vanilla.
- **SEC01**: cero `Send(..., null)`. **F-03 no se arregla volviendo a `null`.**

## 6. Alcance y fronteras

**Eres dueño de:**
- `scripts/5_Mission/LFPG_RPCServerHandlerImpl.c`
- `scripts/5_Mission/LFPG_NetworkManagerImpl.c`
- `scripts/5_Mission/LFPG_FinishWiringTxn.c` (lo creaste tu)

**Ampliacion respecto a tu ronda anterior, porque puede hacer falta para N-02 y N-01:**
`scripts/5_Mission/LFPG_ElecGraphImpl.c` **queda dentro de tu alcance**. Si el arreglo correcto de
los nodos huerfanos vive ahi, escribelo ahi. Es la leccion de la ronda 1: un veto de reparto no
cambia el comportamiento exigido al sistema.

**NO puedes tocar:**
- `scripts/3_Game/LFPG_FileUtil.c`, `scripts/5_Mission/LFPG_BTCHelper.c`,
  `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`, `scripts/3_Game/LFPG_BTCConfig.c`,
  `scripts/5_Mission/LFPG_MissionInit.c` — otra entrega sin fusionar encima.
- `scripts/4_World/LFPG_SorterView.c`, `LFPG_BTCAtmView.c`, `LFPG_BTCAtmController.c`,
  `LFPG_SorterController.c`, `LFPG_SorterPreviewRow.c`, `LFPG_SorterTagView.c` — idem.
- `config.cpp`.
- `git commit`, `git push`, `git checkout`, `git reset`. **Deja los cambios sin commitear.**

**Sobre el facade `scripts/4_World/LFPG_NetworkManager.c`:** lo dejaste fuera y avisaste de que
podia ser un veto mio. El revisor dice que **no hay evidencia para exigir esa ampliacion de API**
como condicion de cierre. Asi que sigue fuera, y esa decision ya no es un veto ciego: esta
razonada. Si aun asi lo necesitas para cerrar un punto del VERDE, **dilo en el informe** en vez de
quedarte bloqueado.

**Refactor no relacionado: NO.** A `VISTO-DE-PASO`.

## 7. Convenciones de la casa — obligatorias, rompen el build

1. **Cero ternarios `? :`**. 2. **Nada de `++`/`--`**: `i = i + 1`. 3. **Nada de `+=`/`-=`**.
4. **Nada de `foreach`**. 5. **`ref` solo en miembros de clase**, nunca en locales, parametros,
retornos ni typedefs — **es la regla documentada de la casa, y el codigo viejo del fichero la
incumple; no lo imites**. 6. Log por `LFPG_Util.Error/Warn/Info/Debug`, nunca `Print()`.
7. **Dos escapes adyacentes en un literal revientan el modulo `World`** con
`CParser: quoted string not closed`: una llamada por linea, cero escapes. 8. `m_` miembros,
`s_` estaticos, PascalCase metodos, camelCase locales. 9. Los modulos se declaran **por carpeta**
en `config.cpp:215-228`: un `.c` nuevo en una carpeta ya declarada se compila solo.

## 8. Permisos y gate

- Shell y escritura **dentro del workspace**. No salgas. **No lances DayZ, AddonBuilder ni builds.**
- **El shell de este host no arranca Python** (hook de PowerShell, `syntax error near unexpected
  token '&'`). Ha mordido a las seis lanes anteriores. Intenta `python enfcheck.py <fichero>`; si te
  lo tumba, dilo y usa la herramienta de busqueda de tu arnes. **No inventes salida de linter**: yo
  la repito y se compara.
- Gates que corres tu y pegas: `grep -c "Send(.*, null)"` en los dos ficheros → **0**; recuento de
  `.Send(` en `LFPG_NetworkManagerImpl.c` → **9** salvo que añadas envios y los justifiques.

## 9. Limites del entorno

- **Enforce no se compila aqui.** El juego compila al cargar; nadie sabra si esto compila hasta que
  yo arranque un servidor. El linter y las convenciones son tu unica proteccion.
- Worktree aislado, rama `fix/t2-r2-transaccion`, con **T2 entero ya aplicado** (rondas 1 y 2).
- El mod corre en **muchos** servidores. `MaxWiresPerDevice` va de 1 a 128; el tope global de nodos
  es 2.048.
- Tope de reloj: **60 minutos**. Prioridad si no llegas: **N-01, F-01 residual, N-02, F-03 residual,
  N-03**. Escribe `REPORT.md` por tramos, no al final.

## 10. Entregable

`REPORT.md` en la raiz, sin commitear. **Empieza por la tabla del VERDE:**

    | # | condicion | ALCANZADO / NO / PARCIAL | evidencia (path:line) |
    V1..V7

Despues, una seccion por punto (N-01, N-02, N-03, F-01 residual, F-03 residual) con:

    - **Que cambie:** con `path:line`
    - **Por que asi:** y que alternativa descartaste
    - **Que NO cubre:** el residuo vivo. Obligatorio
    - **Como comprobarlo:** el experimento concreto in-game

Y las fijas:

    ## LO QUE NO ROMPI
    F-02, SEC01, SEC02, SEC20(a), con evidencia. Es V7.

    ## SALIDA DEL LINTER Y DE LOS GATES

    ## LO QUE NO PUDE VERIFICAR

    ## LA PREMISA DE ESTE ENCARGO
    Casilla obligatoria, y en esta ronda pesa mas que nunca porque es la ultima:
    - El VERDE de la §1, es el listado correcto? Falta una condicion que deberia estar?
    - Hay algun punto del VERDE que sea **imposible** de cerrar sin tocar algo que te he
      vetado, o sin cambiar el formato persistente? Dilo: es preferible saberlo a que lo
      des por cerrado.
    - Dos rondas han cambiado defectos por otros defectos. Ves un motivo estructural para
      que eso siga pasando? Si crees que este tramo necesita un rediseño en vez de una
      tercera pasada, **esa es una respuesta valida y quiero leerla.**

    ## VISTO-DE-PASO

## 11. Quien te revisa

El mismo revisor de las dos rondas anteriores, que conoce cada escenario que escribio y va a
comprobar el VERDE punto por punto. Yo abro cada `path:line` que cites y repito los gates. Una cita
plausible pero inventada invalida el hallazgo: **no cites de memoria, abre el fichero.**
