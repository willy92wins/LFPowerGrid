# BRIEF — Ronda de correccion T1+T6 (ronda 2 de 3)

Fecha: 2026-09-08. Tu lane: `gpt-6-astra`. Rol: **implementador**.
Un revisor de otra familia (Grok) audito el codigo en la ronda 1 y **volvera a auditar lo que tu
escribas** en la ronda 3. No hay ronda 4.

---

## 0. Donde estas y que puedes tocar

Tu workspace es `/tmp/lfpg-astra-r2`. Contiene:

| ruta | que es | permiso |
|---|---|---|
| `scripts/` | el codigo del mod, 152 ficheros `.c` | **editable** |
| `config.cpp` | configuracion del mod | solo lectura, para consultar |
| `.baseline/scripts/` | copia intacta del codigo ANTES de tu ronda | **no tocar**, la puerta la usa para saber que cambiaste |
| `vanilla/` | fuentes de DayZ (2.805 `.c`) | solo lectura, **usalo para verificar firmas** |
| `INFORME.md` | lo escribes tu al final | **crear** |

**No toques nada fuera de `scripts/` y `INFORME.md`.** No borres ficheros. No reorganices.

Esto NO es el repo: es una copia en ext4. El receptor traera de vuelta solo los `.c` que cambien
y los aplicara el mismo. No intentes hacer git aqui; no hay repo.

## 1. Contexto que cambia como escribes

- **DayZ, Enforce Script**, mod corriendo en un **servidor privado con jugadores reales**.
  El dinero del mod es dinero real para ellos.
- **Enforce solo compila al cargar el mundo.** Aqui no hay compilador. Un error de tipado no da
  error ahora: tumba el modulo Mission entero cuando el servidor arranque. Escribe conservador.
- **Verifica firmas contra `vanilla/`, no contra tu memoria.** Si usas un metodo del engine,
  abrelo en `vanilla/` y comprueba la firma. Citalo en el informe con `path:line`.

### Convenciones Enforce de este repo — la puerta las comprueba automaticamente

- **Cero ternarios.** Nada de `a ? b : c`.
- **`x = x + 1`**, nunca `++` ni `--`.
- **Sin `+=` ni `-=`.**
- **Sin `foreach`.** Bucles indexados.
- **`ref` solo en miembros de clase**, nunca en locales.
- **`LFPG_Util.Error/Warn/Info/Debug`**, nunca `Print()`.

Se comprueban sobre las **lineas que anadas**. Medido sobre el codigo existente: 0 ternarios,
0 `++`, 0 `foreach` en 81.462 lineas. El estilo es real, no decorativo.

### Restriccion de modulos — esto te ahorra un arranque roto

`config.cpp` declara el orden de compilacion: **`3_Game` -> `4_World` -> `5_Mission`**
(lineas 218, 223, 228). Un modulo **solo ve a los anteriores**.

**Consecuencia directa para la correccion C5:** `LFPG_BTCAtm.c` esta en `4_World` y
`LFPG_BTCHelper` esta en `5_Mission`. **`LFPG_BTCAtm` NO puede llamar a `LFPG_BTCHelper`**, ni a
su helper `SpawnOnGroundNear`. Verificado: hoy `LFPG_BTCAtm.c` solo referencia `LFPG_AtmStock`
(4_World), `LFPG_BTCConfig` (3_Game), `LFPG_PlayerRPC`, `LFPG_PortDir` y `LFPG_Util`. No rompas
esa propiedad.

## 2. Las cinco correcciones

Todas las citas de abajo **las he verificado yo abriendo el fichero**. Los numeros de linea son
del estado actual del arbol que tienes. Aun asi, **reabre cada una antes de editar**: si algo no
coincide, dilo en el informe en vez de adivinar.

---

### C1 — `TryReadSellDestroyIntent` debe tolerar el terminador de linea

**Donde:** `scripts/3_Game/LFPG_FileUtil.c`, funcion `TryReadSellDestroyIntent` (~:590-640).

**El problema.** El marcador se escribe con `FPrintln` (`:557-561`, cinco lineas: uid,
balanceBefore, creditAmount, btcAmount, classname) y se lee con `FGets`. Despues se compara
**estricto**: `if (lineUid != uid || lineClass == "") return false;`.

Si `FGets` devuelve la linea con un `\r` pegado (fichero escrito en Windows), `lineUid != uid`
es **siempre cierto** y `ParseNonNegativeIntText` falla en las tres numericas. Resultado: el
diario de intencion de venta **nunca** se lee, o sea que la reconciliacion de E04 no corre nunca
y el crash entre credito y destruccion deja al jugador con los objetos Y el saldo.

**No se ha podido determinar si `FGets` deja el `\r`**: no hay ningun round-trip
`FPrintln`->`FGets` probado en produccion en este mod (los dos unicos `FGets` son nuevos), la
declaracion vanilla (`vanilla/1_core/proto/ensystem.c:501`) no lo documenta, y no hay servidor
disponible.

**Que quiero que hagas: que deje de importar.** Normaliza cada linea leida antes de usarla —
quitar `\r`, `\n` y espacios de los extremos. Aplica a las cinco. Con eso el round-trip es
correcto sea cual sea el comportamiento del engine.

**Cuidado:** `classname` puede contener caracteres validos; recorta solo los extremos, no toques
el interior. Comprueba en `vanilla/` que el metodo de recorte que uses existe con esa firma para
`string` en Enforce — si no existe uno, escribe el recorte a mano con un bucle indexado.

---

### C2 — El default de `LFPG_BlocksDismantle` es fail-open, y su comentario dice lo contrario

**Donde:** `scripts/4_World/lfpg_devicebase.c:610-619`.

**El problema, en dos mitades.**

**(a) El comentario miente.** Dice literalmente *"Fail-closed on purpose: a device that cannot
answer must not be dismantled with value inside"*, y justo debajo el cuerpo es `return false`,
que es fail-**open**: por defecto nada bloquea. Las dos frases no pueden ser ciertas a la vez.
**Reescribe el comentario para que describa lo que el codigo hace** — el default es abierto a
proposito, para que los dispositivos que solo tienen cables y attachments no cambien de
comportamiento —, y quita la frase de fail-closed o reformulala para que no contradiga al cuerpo.
No cambies el `return false`.

**(b) Dos dispositivos con valor quedan desprotegidos.** El unico `override` que existe hoy es el
del ATM (`LFPG_BTCAtm.c:82`, verificado por grep: no hay ningun otro). Pero estos dos guardan
valor solo en campos, que el kit no se lleva:

- `scripts/4_World/LFPG_Battery.c:70` — `protected int m_StoredEnergyX10 = 0;`
- `scripts/4_World/LFPG_Furnace.c:61` — `protected int m_FuelCurrent = 0;`

Anade a cada uno un `override bool LFPG_BlocksDismantle()` que devuelva true cuando su campo sea
mayor que 0, con el mismo patron que el ATM.

**No toques `LFPG_BatteryAdapter`**: ya anula el kit (`LFPG_BatteryAdapter.c:85`,
`LFPG_GetKitClassname` vacio) y por tanto no se desmonta.

---

### C3 — El marcador se desarma aunque la destruccion haya quedado a medias

**Donde:** `scripts/5_Mission/LFPG_BTCHelper.c:1523-1534`, en `ReconcilePendingAccountSell`.

**El problema, verificado literal.** El codigo hace:

```
int destroyed = DestroyPlayerItems(player, classname, btcAmount);
if (destroyed != btcAmount)
{
    ... LFPG_Util.Error(shortMsg);   // solo loguea
}
ClearSellDestroyIntentAfterDestroy(uid, current, creditAmount, btcAmount, classname);
```

O sea: si solo se destruyo una parte, se loguea **y se desarma el marcador igual**. La obligacion
pendiente se pierde: nadie volvera a mirarla.

**Que quiero que hagas:** no desarmar el marcador cuando `destroyed != btcAmount`. Deja el
fichero en su sitio y el Error ya existente nombrando el `.sell` al admin. Cuando
`destroyed == btcAmount`, el comportamiento no cambia.

**Piensa antes de escribir**, y dilo en el informe: dejar el marcador armado significa que la
proxima reconciliacion en ESTE arranque entrara por la guarda de `LFPG_SellDestroyedThisBoot`
(`:1512-1516`) y saldra fail-closed sin destruir — eso es lo que queremos. Comprueba que asi es
y que no creas un bucle.

---

### C5 — Un ATM con saldo al que matan a tiros pierde el dinero

**Donde:** `scripts/4_World/LFPG_BTCAtm.c`, `override void LFPG_OnKilled()` en `:456-465`.

**El problema.** T6 cerro el desmontaje con destornillador, pero no la muerte por dano. Verificado:
`config.cpp` da `hitpoints = 200` a `LFPG_BTCAtm` y a `LFPG_BTCAtmAdmin`; `EEKilled`
(`lfpg_devicebase.c:273`) llama a `LFPG_OnKilled()` (`:282`); y el override del ATM **solo apaga
`m_PoweredNet`** — no toca `m_BtcStock`, no reembolsa, no suelta nada. Un jugador dispara al
cajero y los BTC de dentro desaparecen. 200 HP es alcance de raid normal, no un caso raro.

**Decision de producto ya firmada por el dueno:** al morir, **el stock se suelta al suelo como
items**, con el mismo tope de 64 entidades que usa el resto del mod. El que raidea se lleva el
dinero; nadie lo pierde en el vacio.

**Materiales verificados para que no inventes:**

- El classname del item BTC sale de la config: `scripts/3_Game/LFPG_BTCConfig.c:45`
  (`string btcItemClassname;`), con default `"Ammo_9x19_25Rnd"` en `:67`. Busca el accesor
  publico que ya usa el resto del codigo — el ATM ya llama a
  `LFPG_BTCConfig.GetMaxBtcPerMachine()`, asi que el patron existe.
- El stock esta en `m_BtcStock` (`LFPG_BTCAtm.c:40`, `protected`, misma clase: acceso directo OK).
  Hay getter `LFPG_GetBtcStock()` en `:72`.
- **Cuidado con `LFPG_SetBtcStock`** (`:87`): NO es una asignacion simple, pasa por
  `LFPG_AtmStock.PrepareStockMutation(...)` (`:102`). Decide con criterio si al morir procede
  usar el setter o escribir el campo, **y explica en el informe por que elegiste lo que elegiste**.
- El patron de spawn en suelo que usa el mod es una sola linea:
  `g_Game.CreateObjectEx(classname, pos, ECE_CREATEPHYSICS)` — asi lo hace
  `LFPG_BTCHelper.SpawnOnGroundNear` (`5_Mission/LFPG_BTCHelper.c:479-485`, con un jitter de
  +-0,15 en X y Z) y asi lo hace `LFPG_ActionDismantleDevice.c:182`. **Copia el patron, no llames
  al helper**: esta en `5_Mission` y tu estas en `4_World` (§1).
- **Los items BTC apilan.** `StageItemsForPlayerUnchecked`
  (`5_Mission/LFPG_BTCHelper.c:639-700`) lee `GetQuantityMax()` del primer item creado y reparte
  el total en pilas de ese tamano con `SetQuantity((float)qty, false, false)`. Si sueltas 40 BTC
  como 40 entidades cuando caben en 2 pilas, estas gastando el presupuesto de entidades para
  nada. Usa la misma tecnica.
- **El tope de 64** es `LFPG_BTC_MAX_ENTITIES_PER_TX` en `5_Mission/LFPG_BTCHelper.c:240`, pero
  esta declarado **`protected static const`** y ademas en otro modulo: **no lo puedes referenciar**.
  Declara tu propia constante en `4_World` con el mismo valor y un comentario que diga de donde
  sale y por que esta duplicada.

**Fronteras:** esto corre en servidor. Mira como el resto del fichero marca el codigo de servidor
(hay `#ifdef SERVER` en `:458`) y respeta esa convencion. Si el stock es 0, no hagas nada. Si
`CreateObjectEx` devuelve null, loguea con `LFPG_Util.Error` y sigue — no dejes que una excepcion
tumbe `EEKilled`.

---

### C4 — Higiene (hazla al final, es la menos importante)

Solo si las anteriores estan hechas y te queda margen:

- **Quitar `ref` de cuatro locales** (la convencion es `ref` solo en miembros):
  `scripts/3_Game/LFPG_BTCConfig.c:485`, `:486`, `:501`;
  `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1987`, `:2108`.
  Verificado: los cinco son `ref LFPG_BTCCurrency` / `ref LFPG_BalanceData` en locales.
- **Borrar `DestroyPlayerCash`** (`scripts/5_Mission/LFPG_BTCHelper.c:2412`). Verificado por grep:
  en todo `scripts/` solo aparece la definicion, cero call-sites. Ocupa arena de script.

Si al borrarla algo deja de resolver, **no la borres** y dilo en el informe.

## 3. Lo que NO vas a tocar en esta ronda

El revisor lo planteo y esta decidido. No lo reabras:

- **E16 / tombstones que crecen** (`LFPG_BalanceProvider_NativeImpl.c:1388-1395`). Que ya NO se
  pode a los 3 arranques es **deliberado**: es el fail-closed que cierra la duplicacion. Se queda.
- **SEC09 residual** (`LFPG_FileUtil.c:264-292`, `:808-837`). Exige cuatro E/S fallidas
  encadenadas mas un reinicio, y ya esta logueado. Un contrato
  `committed/aborted/uncertain` no cabe en una ronda.
- **`SelectPreparedCashExact` greedy** (`LFPG_BTCHelper.c:91-118`). No pierde dinero; rechaza
  combinaciones exactas no greedy. Un solver de subset-sum en Enforce no es un parche minimo.
- **El caso de clear+rebase fallidos + reinicio.** No hay escritura durable que sobreviva a un
  disco que acaba de rechazar dos escrituras. Es operativo, no de codigo.

## 4. Entregable

Escribe `INFORME.md` en la raiz del workspace, con estas dos secciones **literales**:

```
## HALLAZGOS ATENDIDOS

### C1 — <titulo>
Que cambiaste, en que fichero y por que. Cita `path:line` del estado FINAL.
Si tomaste una decision de diseno, dila y justificala.

### C2 — ...
(una subseccion por correccion que hayas hecho; si NO hiciste alguna, dilo aqui con el motivo)

## LO_NO_VERIFICADO
- Todo lo que asumiste sin comprobar, en bullets.
- Como minimo: que no compilaste, porque aqui no hay DayZ.
```

## 5. La puerta de cierre

Al terminar se ejecuta `python3 gate_astra.py` en la raiz del workspace. Comprueba:

1. `INFORME.md` existe, tiene las dos secciones, al menos un hallazgo `###` con cuerpo, y
   `LO_NO_VERIFICADO` con bullets.
2. Al menos un `.c` cambio de verdad respecto a `.baseline/`.
3. Las **lineas que anadiste** respetan las convenciones Enforce de §1.

Si falla, te dice exactamente que. **La puerta no compila nada**: eso lo hace el receptor con
AddonBuilder y un arranque real. Pasar la puerta no significa que tu codigo funcione.

## 6. Honestidad

- **Cada `path:line` que escribas lo voy a abrir.** Una cita que no existe invalida el punto.
- **Si no hiciste algo, dilo.** Cuatro de cinco correcciones bien hechas y declaradas valen mas
  que cinco a medias.
- **Etiqueta los supuestos.** "Asumo que `SetQuantity` acepta un float" es valido; afirmarlo sin
  abrir `vanilla/` no.
- **Disentir esta bien.** Si crees que una de las cinco correcciones es un error, impleméntala
  igual salvo que sea peligrosa, y escribe tu objecion en el informe. Si es peligrosa, NO la
  hagas y explica por que.
