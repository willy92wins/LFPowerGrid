# BRIEF — Re-auditoria de la correccion + inserciones de T5 (ronda 3 de 3, ULTIMA)

Fecha: 2026-09-08. Lane: `cursor-grok-4.6-xhigh`. Rol: **revisor**.

---

## 1. Donde estamos

Tu auditoste este codigo en la ronda 1 y diste **VERDE, cero GRAVE**, con 6 MEDIO y 3 MENOR.
Tu dictamen esta en `reviews/2026-09-08-t1-t6-revision-cruzada/DICTAMEN-GROK-R1.md`.

**Verifique tus 12 citas abriendo cada fichero. Las 12 eran exactas.** Por eso el dictamen se
tomo en serio y se actuo sobre el.

Un implementador de otra familia (`gpt-6-astra`) escribio la ronda de correccion. Ya esta
commiteada en `9d2d06a`. **Esta es la ULTIMA ronda: no hay ronda 4.** Si queda un GRAVE
despues de esto, se archiva con ficha y lo decide el dueno.

## 2. Que tienes que auditar — DOS cosas

### (A) La correccion — 148 inserciones / 92 borrados en 8 ficheros

Diff completo: `reviews/2026-09-08-t1-t6-revision-cruzada/PATCH-RONDA2-ASTRA.diff`
Informe del implementador: `reviews/2026-09-08-t1-t6-revision-cruzada/INFORME-ASTRA-R2.md`

**Audita el ESTADO FINAL en el arbol, no solo el diff.**

| # | que dice cerrar | ficheros |
|---|---|---|
| C1 | `TryReadSellDestroyIntent` normaliza las 5 lineas con `Trim()` | `3_Game/LFPG_FileUtil.c` |
| C2 | comentario fail-open corregido + overrides en Bateria y Horno | `4_World/lfpg_devicebase.c`, `LFPG_Battery.c`, `LFPG_Furnace.c` |
| C3 | no desarmar el marcador si `destroyed != btcAmount` | `5_Mission/LFPG_BTCHelper.c` |
| C5 | ATM muerto por dano suelta el stock al suelo | `4_World/LFPG_BTCAtm.c` (+110 lineas) |
| C4 | `ref` fuera de 5 locales, `DestroyPlayerCash` borrada | `3_Game/LFPG_BTCConfig.c`, `5_Mission/LFPG_BalanceProvider_NativeImpl.c`, `LFPG_BTCHelper.c` |

**C5 es el que mas superficie nueva trae y donde mas quiero que aprietes.** Preguntas concretas:

- **¿Compila?** Enforce solo compila al cargar el mundo; un error de tipado tumba Mission entero.
  Esto NO se ha compilado. Si ves algo que no compila, es **GRAVE automatico**.
- El codigo comprueba `if (!item)` **despues** de `item.SetQuantity(...)`, con el comentario de que
  SetQuantity puede borrar el item. ¿Es valido en Enforce comprobar asi una referencia a una
  entidad ya borrada, o es un uso-despues-de-liberar? Abre vanilla.
- `array<EntityAI> drops` es un local **sin `ref`** (la convencion de la casa es `ref` solo en
  miembros). ¿Sobrevive el array a la duracion del hook, o puede recolectarse?
- El guard `m_BtcKillDropHandled` es un campo de instancia y **no se persiste**. ¿Hay algun camino
  por el que `LFPG_OnKilled` corra dos veces sobre instancias distintas del mismo ATM?
- El rollback borra las pilas si `LFPG_RemoveBtcStock` falla. ¿Y si el borrado falla? ¿Queda
  dinero duplicado (items en el suelo Y stock en la maquina)?
- ¿El orden importa? Se spawnea primero y se descuenta despues. Si el servidor cae entre medias,
  ¿que queda?
- `EEKilled` -> `LFPG_OnKilled`: ¿el objeto sigue teniendo posicion valida cuando corre esto?
  ¿Y si lo mata algo que lo borra inmediatamente?

Para C1, C2, C3, C4 basta con que confirmes que cierran lo que dicen y **no rompen nada**.

### (B) Las inserciones de T5 en el camino del dinero — NUNCA revisadas

Diff: `reviews/2026-09-08-t1-t6-revision-cruzada/T5-INSERCIONES-CAMINO-DINERO.diff`

T5 es un instrumento de inyeccion de fallos. **No estaba en la unidad que auditaste en la ronda 1**,
y el dueno ha decidido fusionarlo a `main`. Mete condicionales dentro del camino del dinero, asi:

```
if (!LFPG_FaultInject.ShouldFail("E04_clear_after_destroy"))
    cleared = LFPG_FileUtil.ClearSellDestroyIntent(uid);
```

Lo que quiero saber, y **solo** esto:

1. **¿Puede `ShouldFail` / `ShouldCrash` devolver true en un servidor de produccion?** Se midio que
   el inyector arranca `DISARMED reason=no_file`, o sea que depende de que exista un JSON. ¿Que
   pasa exactamente si ese fichero aparece por accidente, o si el parseo falla a medias?
2. **¿Alguna de esas ramas deja el camino del dinero en peor estado que sin T5**, incluso desarmado?
   Fijate en si el valor por defecto de una variable cambia cuando la llamada se salta.
3. **¿Choca semanticamente con las correcciones C1/C3?** T5 y la correccion tocan el mismo fichero.

No audites el resto de T5 (el JSON de escenarios, el driver Python, `LFPG_FaultInject.c` entero).
Solo sus inserciones en `LFPG_FileUtil.c`, `LFPG_BTCHelper.c` y `LFPG_BalanceProvider_NativeImpl.c`.

## 3. El VERDE — el mismo de la ronda 1, sin mover

> **Cero GRAVE vivo en el camino del dinero, y cada GRAVE que declares lleva su cita `path:line`
> verificable.**

- **GRAVE** = el jugador pierde o duplica valor; el servidor corrompe estado monetario; o **no compila**.
- **MEDIO** = degradacion, mensaje enganoso, deuda que no cuesta dinero hoy.
- **MENOR** = estilo, codigo muerto, friccion.

Un escenario que exige un fallo de hardware Y un reinicio en una ventana de milisegundos Y una
accion del jugador **no es GRAVE por si solo**: clasificalo y cuantifica cuantas cosas tienen que
alinearse. En la ronda 1 hiciste esto bien; sigue igual.

## 4. Lo que NO se relitiga

Ya decidido, no gastes turnos:

- **Los 6 MEDIO y 3 MENOR de tu ronda 1 que quedaron fuera de alcance** — E16/tombstones que crecen,
  SEC09 residual, greedy no subset-sum, clear+rebase+reinicio. Estan **aceptados por escrito**. No
  los repitas salvo que la correccion los haya EMPEORADO.
- **La decision de producto de C5**: el dueno firmo que el stock caiga al suelo. Si crees que la
  implementacion esta mal, dilo; que la decision sea otra, no.
- **El gate de compilacion de T1/T2/T0a/T5/T6 esta VERDE** (2026-09-07). Lo que NO esta compilado
  es la correccion de hoy.
- **Nada in-game.** No tienes servidor. No propongas "hay que probarlo" como hallazgo; ya lo sabemos.

## 5. Formato de salida — obligatorio

**No puedes escribir ficheros** (modo lectura). Devuelve el dictamen en tu mensaje final:

```
# DICTAMEN RONDA 3 — correccion + inserciones T5

## VEREDICTO
<VERDE | ROJO>. Una frase. Si es ROJO, cuantos GRAVE y en cual de las dos partes.

## (A) LA CORRECCION
### [GRAVE|MEDIO|MENOR] <titulo>
- **Donde:** path:line (obligatorio y tiene que existir)
- **Que pasa:**
- **Escenario de fallo:** entradas/estado concretos -> resultado incorrecto
- **Correccion propuesta:** o "no cabe, archivar con ficha"
- **Confianza:** CONFIRMADO | PLAUSIBLE | ESPECULATIVO

### VEREDICTO POR CORRECCION
Una linea por cada C1..C5: CIERRA / CIERRA PARCIAL / NO CIERRA / EMPEORA, con motivo.

## (B) INSERCIONES DE T5
Mismo formato. Si no hay nada, dilo explicitamente.

## LO QUE HE VERIFICADO
## LO QUE NO HE PODIDO VERIFICAR
```

## 6. Honestidad

- **Cada `path:line` lo voy a abrir**, como en la ronda 1.
- **No anuncies verificaciones que no hiciste.**
- **Etiqueta los supuestos.**
- **Si la correccion esta bien, dilo sin adornos.** Un VERDE honesto en la ultima ronda vale mas
  que un hallazgo estirado para justificar la ronda.
