# BRIEF — Auditoría adversarial de T1 (integridad monetaria) + T6 (guard de desmontaje)

Ronda 1 de 3. Fecha: 2026-09-08. Lane: `cursor-grok-4.6-xhigh`. Rol: **revisor**, no implementador.

---

## 1. Qué decide esta ronda

Este código **no lo ha revisado nadie en su estado actual**. Está commiteado, compila y arranca,
pero el único dictamen que existe (`REVIEW-T1.md`) es de un estado ANTERIOR y quedó desfasado a los
30 minutos. Tu dictamen es lo que decide si esto se fusiona.

**No estás aquí para aprobar.** Estás aquí para encontrar lo que rompe. Si no encuentras nada
GRAVE, dilo con esa misma claridad y justifica por qué el camino del dinero es correcto — un
"parece bien" sin recorrer los caminos de fallo no vale.

## 2. EL VERDE — fijado ANTES de empezar, y no se mueve

Esto es lo único que decide si el tramo cierra:

> **Cero GRAVE vivo en el camino del dinero, y cada GRAVE que declares cerrado lleva su cita
> `path:line` verificable.**

Definiciones, para que no haya deslizamiento de listón:

- **GRAVE** = el jugador pierde valor, duplica valor, o el servidor corrompe/pierde estado
  monetario persistente. También: el módulo no compila, o una excepción tumba el flujo.
- **MEDIO** = degradación, mensaje engañoso, fail-open en algo no monetario, deuda que no cuesta
  dinero hoy.
- **MENOR** = estilo, código muerto, fricción.

**Un escenario que exige un fallo de hardware Y un reinicio en una ventana de milisegundos Y una
acción del jugador no es GRAVE por sí solo: clasifícalo, cuantifica lo que hace falta que se
alinee, y déjalo en MEDIO salvo que puedas argumentar que es alcanzable en operación normal.**
El tramo anterior de este proyecto costó dos rondas enteras porque el revisor siempre podía
encontrar un escenario más. No repitas eso.

## 3. Presupuesto de rondas — léelo antes de escribir un hallazgo

Hay **exactamente una ronda de corrección** después de la tuya. Otro modelo, de otra familia,
implementará lo que tú marques como GRAVE, y luego tú lo re-auditas una vez. **No hay ronda 4.**

Consecuencia práctica para ti: **prioriza**. Un hallazgo GRAVE que no se puede corregir en una
pasada acotada es un hallazgo que hay que declarar como tal ("esto no cabe en una ronda, hay que
archivarlo con ficha"). Prefiero cinco GRAVE accionables que veinte hallazgos donde los buenos se
pierden entre los teóricos.

## 4. La unidad revisable

Ocho ficheros, 1.178 inserciones / 181 borrados. El diff completo está en este mismo directorio:

```
reviews/2026-09-08-t1-t6-revision-cruzada/UNIDAD-REVISABLE.diff
```

Ficheros, con su tramo:

| fichero | tramo |
|---|---|
| `scripts/3_Game/LFPG_BTCConfig.c` | T1 |
| `scripts/3_Game/LFPG_FileUtil.c` | T1 |
| `scripts/5_Mission/LFPG_BTCHelper.c` | T1 **y** T6 (los dos tocan este fichero) |
| `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c` | T1 |
| `scripts/5_Mission/LFPG_MissionInit.c` | T1 |
| `scripts/4_World/LFPG_ActionDismantleDevice.c` | T6 |
| `scripts/4_World/LFPG_BTCAtm.c` | T6 |
| `scripts/4_World/lfpg_devicebase.c` | T6 |

**Audita el ESTADO FINAL en el árbol, no solo el diff.** El diff te dice qué cambió; el estado
final es lo que corre. Los ficheros están en el workspace, léelos.

⚠ **Trampa de rutas**: los documentos de insumo (fichas y dictamen viejo) citan
`scripts/4_World/LFPG_BTCHelper.c` y `LFPG_BalanceProvider_Native.c`. **Esas rutas ya no existen.**
Hoy son `scripts/5_Mission/LFPG_BTCHelper.c` y `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c`.
Los números de línea de esos documentos son del commit viejo y **no sirven**: re-localiza por
nombre de símbolo, nunca por número heredado.

## 5. Contexto de dominio — por qué esto importa

- Es un mod de **DayZ** en **Enforce Script**, corriendo en un **servidor privado con jugadores
  reales ahora mismo**.
- El dinero del mod (BTC y efectivo) es dinero real para esos jugadores. **La pérdida de dinero
  acumula daño por sesión de juego**, no es deuda latente.
- Enforce **solo compila al cargar el mundo**. Un error de tipado tumba el módulo Mission entero
  al arrancar. Si ves algo que no compila, es GRAVE automático.
- Dos secuencias de escape adyacentes en un literal rompen el parser con
  `CParser: quoted string not closed`. Si ves un literal sospechoso, márcalo.

## 6. Convenciones Enforce obligatorias de este repo

El repo **no tiene `AGENTS.md`**, así que van aquí. Una violación es MENOR salvo que cambie
comportamiento:

- **Cero ternarios.** Nada de `a ? b : c`.
- **`x = x + 1`**, nunca `++` ni `--`.
- **Sin `+=` ni `-=`.**
- **Sin `foreach`.** Bucles indexados.
- **`ref` solo en miembros de clase**, no en locales.
- **`LFPG_Util.Error/Warn/Info/Debug`**, nunca `Print()`.

## 7. Lo que este código AFIRMA cerrar — ponlo a prueba, no te lo creas

Estas son afirmaciones del implementador. Tu trabajo es intentar romperlas.

**E04 — venta a cuenta pierde BTC si falla el guardado del crédito tras destruir los objetos.**
Afirmación: hay diario de intención durable (`WriteSellDestroyIntent`) escrito ANTES del
`AddBalance`, y reconciliación en cada conexión (`LFPG_MissionInit.c`, `InvokeOnConnect`) que
compara el saldo: `before+credit` → destruye; `before` → limpia; otro valor → no toca items.
Más un re-armado: si el borrado del marcador falla, se reescribe con el saldo post-crédito.
Más (T6) un mapa en memoria `s_SellDestroyedThisBoot` que sale fail-closed si el marcador
sobrevive a una venta ya destruida en este arranque.
**Preguntas para ti**: ¿la máquina de estados cubre todas las transiciones? ¿Qué pasa si el
marcador queda ilegible a medias? ¿Y si el jugador se reconecta dos veces? ¿Y si el saldo cambió
por otra vía entre la venta y la reconexión, dando un tercer valor legítimo?

**E16 — poda de tombstone antes de probar persistencia.** Afirmación: la poda **ya no existe**;
se devuelve `false` sin podar al llegar a 3 boots. **Preguntas**: ¿queda alguna otra vía que pode?
¿El tombstone que ahora se conserva crece sin límite?

**SEC09 — un guardado que devuelve fallo puede confirmarse en el siguiente arranque** (un `.tmp`
válido sobrevive a un `CopyFile` fallido y se promociona luego). Afirmación: PARCIAL, no cerrada.
**Pregunta**: ¿sigue viva la vía? ¿Está al menos fail-closed?

**E02, E03, E05, E08** — compra en efectivo que crea entidades antes de comprobar pago; cap de 64;
cambio greedy no óptimo; validación del catálogo de monedas. Afirmadas CERRADAS.

**T6 — guard de desmontaje.** Un ATM con saldo se podía desmontar y su dinero desaparecía sin
aviso ni reembolso. Afirmación: virtual `LFPG_BlocksDismantle()` en `lfpg_devicebase.c` con
default `false`, `override` en `LFPG_BTCAtmBase` devolviendo `m_BtcStock > 0`, y comprobación en
`LFPG_ValidateDismantle`.
**Preguntas**: ¿hay OTRO camino que borre el dispositivo sin pasar por `LFPG_ValidateDismantle`?
¿El default `false` deja algún otro dispositivo con valor desprotegido? ¿Y si el ATM se destruye
por daño, por limpieza del servidor, o por el kit desde el otro lado?

## 8. Insumos — útiles, pero ninguno es verdad establecida

| documento | qué es | aviso |
|---|---|---|
| `reviews/2026-09-06-council-auditorias/REVIEW-T1.md` | dictamen adversarial previo | **DESFASADO.** Es de las 01:22; el código siguió cambiando hasta las 01:56. Sus dos GRAVE (E04, E16) ya no aplican tal como los describe. Úsalo para saber dónde mirar, **nunca** como cobertura |
| `reviews/2026-09-06-council-auditorias/IMPL-T1-INFORME.md` | informe del implementador, con §`LO_NO_VERIFICADO` | Esa sección es oro: dice dónde él mismo sabe que no llegó |
| `reviews/2026-09-06-council-auditorias/T1-fichas.md` | las fichas originales de auditoría | Líneas del commit viejo, ver §4 |
| `reviews/2026-09-06-council-auditorias/snapshot-antes-de-R2.diff` | el estado que SÍ revisó `REVIEW-T1.md` | Compáralo si quieres saber qué es nuevo desde entonces |

## 9. Lo que NO se relitiga en esta ronda

No gastes turnos aquí. Está medido y decidido:

- **El gate de compilación está VERDE.** T1, T2, T0a, T5 y T6 compilan y arrancan, verificado el
  2026-09-07 con despliegue anterior al arranque y discriminador de contenido. No pidas "compilar
  una vez antes de tocar el servidor" como hallazgo: ya se hizo.
- **Los defaults `Paper_Bill_*` no existen en vanilla** — medido, 0 hits en 124 PBO. El catálogo
  sale vacío **a propósito** y el efectivo queda fail-closed hasta que el admin configure. Es una
  decisión de producto firmada, no un bug.
- **El `LF_BTCAtm.json` de producción no está en este disco** y el mod corre en muchos servidores.
  El gate no es "revisar ese fichero", es "que el código aguante ficheros que nunca veremos".
- **D16** (no se comprueba propiedad ni candado en el emparejado) es **riesgo aceptado** por el
  dueño, no un hallazgo.
- **Nada in-game.** No tienes servidor. No propongas como hallazgo "hay que probarlo en juego";
  ya sabemos que hay que probarlo. Si un hallazgo SOLO se puede decidir in-game, dilo y clasifícalo
  como tal en la sección correspondiente.

## 10. Formato de salida — obligatorio

**No puedes escribir ficheros** (estás en modo lectura). Devuelve el dictamen entero en tu mensaje
final, con esta estructura exacta:

```
# DICTAMEN T1+T6 — ronda 1

## VEREDICTO
<VERDE | ROJO>. Una frase. Si es ROJO, di cuántos GRAVE.

## HALLAZGOS
### [GRAVE|MEDIO|MENOR] <título de una línea>
- **Dónde:** path:line (obligatorio, y tiene que existir)
- **Qué pasa:** el defecto, en dos o tres frases
- **Escenario de fallo:** entradas/estado concretos -> resultado incorrecto. Si hace falta que
  se alineen N cosas, dilas todas y numéralas.
- **Corrección propuesta:** acotada, implementable en una pasada. Si no cabe, dilo.
- **Confianza:** CONFIRMADO (lo he leído y el camino es cierto) | PLAUSIBLE (razonado, no
  cerrado) | ESPECULATIVO

## LO QUE HE VERIFICADO
Qué ficheros abriste, qué grep corriste, qué caminos recorriste entero.

## LO QUE NO HE PODIDO VERIFICAR
Explícito. Todo lo que asumiste sin comprobar va aquí.
```

## 11. Honestidad — esto se comprueba

- **Cada `path:line` que escribas lo voy a abrir.** Una cita que no existe, o que no dice lo que
  afirmas, invalida el hallazgo entero.
- **No anuncies verificaciones que no hiciste.** Esta lane tiene antecedentes medidos de abrir con
  "voy a consultar las definiciones vanilla" y luego responder de memoria. Si no lo abriste, dilo.
- **Etiqueta los supuestos.** "Asumo que `CreateInInventory` no fusiona pilas" es una frase
  perfectamente válida. "`CreateInInventory` no fusiona pilas" sin haberlo comprobado, no.
- **Disentir está bien.** Si crees que una decisión firmada por el dueño es un error, dilo en
  MEDIO con su argumento. Lo que no vale es reabrirla como si nadie la hubiera decidido.
