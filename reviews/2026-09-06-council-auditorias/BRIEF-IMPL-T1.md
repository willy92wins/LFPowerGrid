# ENCARGO DE IMPLEMENTACIÓN — T1: integridad monetaria (LFPowerGrid)

## 1. La tarea en una frase
Cerrar los caminos por los que una operación de dinero del mod puede destruir valor del jugador o
resucitar una mutación revertida, en un servidor que **tiene jugadores ahora mismo**.

## 2. Rol y criterio de éxito
Eres el **implementador**. Produces código, no propuestas. Tu trabajo lo revisará después un
modelo de otra familia, así que cada cambio tiene que poder defenderse solo con el diff y tus notas.

Éxito = las 7 fichas de abajo cerradas o explícitamente declaradas como no cerradas y por qué, con
el árbol compilando y sin cambiar comportamiento fuera de lo que cada ficha describe.

## 3. Entorno — léelo, no heredas nada de la sesión que te encarga
- Repo en `P:\LFPowerGrid` (unidad de trabajo de DayZ; también accesible como
  `C:\Users\guill\OneDrive\Documentos\DayZ Projects\LFPowerGrid`). Es tu workspace.
- **Enforce Script de DayZ**, ficheros `.c`. **No es C ni C++.** No hay excepciones, no hay
  `try/catch`, no hay plantillas al uso, no hay `nullptr` — se usa `null` y comprobación explícita.
- **La rama ya está creada y activa: `fix/t1-integridad-monetaria`, basada en `421cabb`.**
- **PROHIBIDO TOCAR GIT.** Nada de `commit`, `add`, `checkout`, `branch`, `stash`, `merge`,
  `push`, `reset`. Deja los cambios en el árbol de trabajo y ya está. Quien encarga decide el commit.
- **PROHIBIDO** compilar con AddonBuilder, empaquetar PBO, lanzar DayZ o tocar
  `P:\LFPowerGrid_dev\`.
- **PROHIBIDO** tocar nada fuera de los ficheros que las fichas nombran. En particular: nada del
  sorter (`LFPG_Sorter*`, `scripts/4_World/test/`), nada de red/RPC salvo lo que una ficha exija,
  y nada de `config.cpp`.

## 4. Convenciones de la casa — verificadas contra el árbol, no de memoria
Estas no son estilo, son **reglas del compilador de Enforce y del proyecto**. Medidas hoy:

- **Cero ternarios `? :`** en todo el árbol. Usa `if`/`else`.
- **Nada de `++` / `--`**: hay 17 residuales frente a **753** `x = x + 1`. Escribe `i = i + 1`.
- **Nada de `+=` / `-=`**. Escribe `total = total + n`.
- **Nada de `foreach`**: las 19 apariciones del árbol son comentarios que lo prohíben. Usa
  `for (int i = 0; i < arr.Count(); i = i + 1)`.
- **Registro:** `LFPG_Util.Error(...)` / `.Warn(...)` / `.Info(...)` / `.Debug(...)`. Es el idioma
  de la casa (255/315/254/125 usos). No uses `Print` ni `PrintFormat`.
- **Regla del escape simple (rompe la compilación de `World` entero si se viola):** nunca juntes
  dos secuencias de escape en un mismo literal. Se construyen por concatenación. Está documentada
  en el propio código, `scripts/4_World/test/LFPG_SorterView_TEST.c:2073-2076`; léela antes de
  escribir cualquier literal con barras o comillas.
- Guardas de plataforma: el código usa `#ifdef SERVER` / `#ifndef SERVER`. Respeta el lado en el
  que ya vive cada función; no muevas lógica de servidor al cliente.

## 5. Frontera de confianza que NO se debilita (G6 — fail-closed)
El servidor es la autoridad. Ninguna corrección puede:
- aceptar como buena una cantidad, un saldo o una identidad que venga del cliente;
- convertir un error en un éxito silencioso: si algo falla, **falla cerrado** y se registra;
- ampliar lo que un jugador puede pedir.
Si para arreglar una ficha necesitas relajar una comprobación, **no lo hagas**: escríbelo en
`LO_NO_VERIFICADO` y sigue con la siguiente.

## 6. Principio rector de este tramo
> **Ninguna ruta de error monetario puede dejar RAM, disco e hive en estados incoherentes.**

En la práctica: **no destruyas nada hasta que lo que lo sustituye esté persistido**, y si no puedes
garantizar el orden, **restituye** lo destruido antes de devolver el error. Un `return` con código
de error no repara por sí solo unos objetos ya borrados.

## 7. Las fichas, por orden de ataque

Cada ficha está en `T1-fichas.md`, en tu workspace, con su evidencia y su prueba de regresión.
**Los números de línea de las fichas son de un commit de agosto y ya no valen**: localiza el código
por nombre de símbolo. Dos ficheros grandes se partieron en fachada + implementación, así que la
lógica que las fichas sitúan en `scripts/4_World/LFPG_NetworkManager.c` y
`.../LFPG_RPCServerHandler.c` vive hoy en `scripts/5_Mission/LFPG_NetworkManagerImpl.c` y
`.../LFPG_RPCServerHandlerImpl.c`.

**Estas cuatro ubicaciones sí están verificadas contra HEAD `421cabb` por quien te encarga:**

| Ficha | Dónde está hoy |
|---|---|
| **E04** | `scripts/5_Mission/LFPG_BTCHelper.c:1465` (destrucción) y `:1505-1513` (crédito fallido) |
| **E16** | `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1230-1243` y `scripts/4_World/LFPG_BTCAtm.c:165-176` |
| **SEC09** | `scripts/3_Game/LFPG_FileUtil.c:580-585` (guarda que YA existe), `:214-218` y `:280-283` (los dos residuos) |
| E02/E03/E08/E05 | `scripts/5_Mission/LFPG_BTCHelper.c` — localízalos por símbolo |

**Orden obligatorio, de más a menos daño:**

1. **E04 — Sell destruye BTC y pierde el crédito.** El bloque es *byte a byte idéntico* al que
   auditaron. Se destruyen los BTC en `:1465`; si `AddBalance` devuelve algo distinto de lo
   previsto, se emite error y se sale **sin restituir**. Además, en esa rama no se llama a
   `AbortOutputs()`, así que el efectivo de derrame por tope de cuenta ya materializado se queda
   con el jugador. Es la única ficha del tramo que destruye valor irreversiblemente en uso normal.
2. **SEC09 — residuo.** La guarda principal ya entró en el commit `ef29b73` y **no hay que
   rehacerla**: hoy `EnsureBalancesFileOrRestore` se niega a promocionar un `.tmp` que convive con
   un target vivo. Quedan dos vías: (a) si la escritura del `.tmp` falla dejando un fichero
   parseable **y no existe target**, la guarda no aplica y el arranque siguiente acredita una
   mutación que ya se revirtió en RAM; (b) el último recurso documentado en `:280-283`. Cierra las
   dos, o cierra una y explica por qué la otra no se puede.
3. **E16 — el tombstone se borra antes de probar que el stock compensado quedó en el hive.** El
   stock va a RAM y se sincroniza, pero solo se vuelve durable por `LFPG_OnStoreSaveExtra`, en otro
   momento y con otro mecanismo, mientras los tombstones se borran de forma durable ya. Fíjate en
   que la rama hermana de compras **sí** conserva las claims PENDING hasta un arranque futuro:
   ahí tienes el patrón correcto ya escrito en el mismo fichero.
4. **E02** — la compra en efectivo crea entidades antes de comprobar que el jugador puede pagarlas.
5. **E03** — los límites monetarios no limitan el número de entidades que genera una transacción.
   Con jugadores reales esto es un vector de inundación del servidor, no solo coste.
6. **E08** — la configuración de monedas admite classnames duplicados, solapados con BTC o
   inválidos. Valida al cargar y falla cerrado.
7. **E05** — `DepositCash` persiste un crédito antes de comprobar que ese importe es representable
   con los billetes disponibles.

## 8. Trampa conocida antes de tocar el orden de commit/abort
La ficha **E15** (también en `T1-fichas.md`, solo como contexto) dice que ocho compras a cuenta
bloquean más compras en el mismo ATM hasta una reconciliación de arranque. Eso significa que **el
arranque asume el orden de commit/abort actual**. Si reordenas Sell o Deposit sin mirar esa
reconciliación, puedes convertir un bloqueo temporal en una pérdida definitiva. Léela antes de
tocar el orden, y si tu cambio afecta a la reconciliación, dilo.

## 9. Cómo entregar
- Edita los ficheros en el árbol. Un cambio acotado por ficha; no mezcles fichas en la misma
  función si puedes evitarlo.
- No hagas refactor de paso. Nada de renombrar, reordenar ni «ya que estoy».
- Escribe un informe en `reviews/2026-09-06-council-auditorias/IMPL-T1-INFORME.md` con, por ficha:

```
### <ID> — CERRADA | PARCIAL | NO_CERRADA
- **Qué cambié:** path:line + descripción en 2-3 frases
- **Por qué así:** la alternativa que descartaste y el motivo
- **Qué NO cubre:** el borde que sigue abierto, si lo hay
- **Cómo comprobarlo:** el escenario concreto que lo verificaría in-game
```

- Termina el informe con `## LO_NO_VERIFICADO`: todo lo que tuviste que suponer, toda API cuya
  firma no pudiste confirmar leyendo, y toda ficha que dejaste sin cerrar. **Sin esa sección el
  encargo no está entregado.** Preferimos una ficha honestamente sin cerrar a una cerrada a ciegas.

## 10. Lo que no debes hacer aunque parezca buena idea
- Inventar una API. Si necesitas una firma, búscala con grep en el árbol y cítala. Si no existe,
  va a `LO_NO_VERIFICADO`.
- Añadir dependencias, ficheros nuevos o clases nuevas sin necesidad: cada clase cuesta arena de
  script, que es un recurso limitado en este mod.
- Ampliar el alcance a fichas P2/P3 que veas de paso (E01, E06, E07, E09…). No entran.
- Tocar el sorter, el renderer, el grafo o los dispositivos. Este tramo es solo dinero.
