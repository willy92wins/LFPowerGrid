## HALLAZGOS ATENDIDOS

Aplicadas C1–C5 en ocho ficheros `.c`. Se conservan los 152 ficheros de `scripts/`. No se modificaron `config.cpp`, `vanilla/`, `.baseline/` ni los asuntos excluidos por el brief. Todas las citas siguientes corresponden al estado final.

### C1 — Normalizar las cinco líneas del marcador de venta

En `scripts/3_Game/LFPG_FileUtil.c:620` a `scripts/3_Game/LFPG_FileUtil.c:624`, las cinco líneas se reasignan con `Trim()` después de leer y cerrar el fichero, antes de comparar UID/classname o analizar números. Se recortan los extremos, no el interior del classname. La lectura incompleta sigue rechazándose.

Firma abierta antes de usarla: `proto string Trim();` en `vanilla/1_core/proto/enstring.c:304`. Su contrato documenta eliminación de whitespace inicial y final en `vanilla/1_core/proto/enstring.c:292`; se usa para espacios y terminadores CR/LF. También se reabrieron `FPrintln(FileHandle file, void var)` en `vanilla/1_core/proto/ensystem.c:481` y `FGets(FileHandle file, string var)` en `vanilla/1_core/proto/ensystem.c:501`. No se presupone que `FGets` retire el terminador.

### C2 — Proteger energía y combustible al desmontar

El comentario de `scripts/4_World/lfpg_devicebase.c:610` describe ahora el default abierto deliberado. El cuerpo conserva `return false` en `scripts/4_World/lfpg_devicebase.c:617`.

Se añadieron los overrides de `LFPG_BlocksDismantle()`:
- `scripts/4_World/LFPG_Battery.c:122`: devuelve `m_StoredEnergyX10 > 0`.
- `scripts/4_World/LFPG_Furnace.c:101`: devuelve `m_FuelCurrent > 0`.

Son métodos del mod, sin nuevas llamadas al engine. `LFPG_BatteryAdapter.c` se comparó byte a byte con baseline y no cambió.

### C3 — Conservar el marcador tras una destrucción incompleta

En `scripts/5_Mission/LFPG_BTCHelper.c:1523`, la rama `destroyed != btcAmount` registra el error y retorna en `scripts/5_Mission/LFPG_BTCHelper.c:1534`. No borra ni rebasa el marcador. Solo la destrucción completa alcanza `ClearSellDestroyIntentAfterDestroy` en `scripts/5_Mission/LFPG_BTCHelper.c:1536`.

Discrepancia observada: el error original de esta rama no nombraba `.sell`. Se añadió esa referencia y la instrucción de reconciliar antes del reinicio en `scripts/5_Mission/LFPG_BTCHelper.c:1531`. El aviso de la guarda también pide inspeccionar, no borrar a ciegas un marcador que ahora puede representar una destrucción parcial.

La marca de este arranque se establece antes de destruir en `scripts/5_Mission/LFPG_BTCHelper.c:1521`. Con el mismo saldo acreditado, la siguiente reconciliación alcanza la guarda de `scripts/5_Mission/LFPG_BTCHelper.c:1512` y retorna sin otra destrucción. Si el saldo cambió, se aplican las ramas previas existentes; no se puede llegar a destruir otra vez pasando esa guarda. No se añadió ningún bucle ni reintento programado. No hay nuevas firmas del engine en esta corrección.

### C5 — Soltar el stock de ambos ATM al morir

La caída está en `LFPG_BTCAtmBase.LFPG_OnKilled`, en `scripts/4_World/LFPG_BTCAtm.c:282`, bajo `#ifdef SERVER`. Así la hereda el ATM admin. El ATM jugador llama a `super.LFPG_OnKilled()` en `scripts/4_World/LFPG_BTCAtm.c:567` y conserva su apagado. El stock cero sale sin crear objetos. Una guarda por instancia impide repetir la entrega en otra llamada al hook.

Se usa `LFPG_BTCConfig.GetBtcItemClassname()`, cuyo accesor está en `scripts/3_Game/LFPG_BTCConfig.c:571`. La constante local de 64 está en `scripts/4_World/LFPG_BTCAtm.c:45`; su comentario explica la duplicación de la constante protegida de Mission, `scripts/5_Mission/LFPG_BTCHelper.c:240`. No se llama a ningún helper de `5_Mission` desde `4_World`.

El bucle de `scripts/4_World/LFPG_BTCAtm.c:309` limita también los intentos fallidos a 64. Copia el patrón `CreateObjectEx(classname, pos, ECE_CREATEPHYSICS)` y el jitter X/Z de ±0,15. La primera entidad válida sirve de probe con `GetQuantityMax()`. Se reparten pilas con `SetQuantity((float)qty, false, false)` y se comprueba la cantidad leída cuando el objeto tiene cantidad. Los objetos sin cantidad cuentan como una unidad. Un spawn null se registra con `Error` y continúa; un objeto de tipo incorrecto o con cantidad incorrecta se solicita borrar y no cuenta como entregado.

**Decisión sobre el stock:** no se usa el setter ni una asignación directa que evada el diario. Se reutiliza `LFPG_RemoveBtcStock(delivered)` en `scripts/4_World/LFPG_BTCAtm.c:363`. Este método ya devuelve `bool`, descuenta exactamente y pasa por `PrepareStockMutation` antes de asignar (`scripts/4_World/LFPG_BTCAtm.c:158`). El setter de `scripts/4_World/LFPG_BTCAtm.c:93` además recorta al máximo configurado actual; eso podría borrar residual cargado bajo un límite antiguo. El forwarder está en `scripts/4_World/LFPG_AtmStock.c:25` y la preparación del diario en `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:519`.

Las pilas se crean primero y solo se descuentan las unidades representadas. Si el descuento se rechaza, se solicita borrar todas las pilas creadas y se registra el stock retenido (`scripts/4_World/LFPG_BTCAtm.c:363`). No hay fallback que salte el diario. Si quedan unidades por el tope o fallos, se mantienen en el campo y el error pide recuperación administrativa antes de eliminar el ATM arruinado (`scripts/4_World/LFPG_BTCAtm.c:379`). Esto no equivale a una recuperación durable garantizada.

Firmas y contratos abiertos en `vanilla/` antes de usarlos:
- `CreateObjectEx(string type, vector pos, int iFlags, int iRotation = RF_DEFAULT)` devuelve `Object`: `vanilla/3_game/global/game.c:702`. `ObjectDelete(Object obj)` devuelve `void`: `vanilla/3_game/global/game.c:704`.
- `ECE_CREATEPHYSICS`: `vanilla/3_game/ce/centraleconomy.c:16`. `GetPosition()` devuelve `vector`: `vanilla/3_game/entities/object.c:293`. `Math.RandomFloat(float min, float max)`: `vanilla/1_core/proto/enmath.c:91`.
- Cast seguro y nullable, `proto static Class Cast(Class from)`: `vanilla/1_core/proto/enscript.c:94`.
- `HasQuantity()` devuelve `bool`, `SetQuantity(float value, bool destroy_config = true, bool destroy_forced = false, bool allow_client = false, bool clamp_to_stack_max = true)` devuelve `bool`, `GetQuantity()` devuelve `float` y `GetQuantityMax()` devuelve `int`: `vanilla/3_game/entities/entityai.c:2237`, `vanilla/3_game/entities/entityai.c:2242`, `vanilla/3_game/entities/entityai.c:2244` y `vanilla/3_game/entities/entityai.c:2256`.
- `SetQuantity` no devuelve éxito: documenta si elimina el item en `vanilla/4_world/entities/itembase.c:3339` y devuelve `false` al terminar normalmente en `vanilla/4_world/entities/itembase.c:3408`. Se abrieron también los overrides de cantidad en `vanilla/4_world/entities/itembase.c:3459`, `vanilla/4_world/entities/itembase.c:3503` y `vanilla/4_world/entities/itembase.c:3508`.
- `array.Count()`, `array.Get(int n)` y `array.Insert(T value)`: `vanilla/1_core/proto/enscript.c:380`, `vanilla/1_core/proto/enscript.c:399` y `vanilla/1_core/proto/enscript.c:407`. `int.ToString()`: `vanilla/1_core/proto/enconvert.c:30`. `SetSynchDirty()`, conservado en el descuento y apagado existentes: `vanilla/3_game/entities/entityai.c:3069`.

### C4 — Higiene final

El brief dice «cuatro locales», pero enumera cinco. Se quitó `ref` de los cinco: `scripts/3_Game/LFPG_BTCConfig.c:485`, `scripts/3_Game/LFPG_BTCConfig.c:486`, `scripts/3_Game/LFPG_BTCConfig.c:501`, `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:1987` y `scripts/5_Mission/LFPG_BalanceProvider_NativeImpl.c:2108`. No se cambian miembros ni contratos.

La búsqueda completa previa de `DestroyPlayerCash` encontró solo su definición. Se eliminó la función; ahora `HandleBTCWithdrawCash` comienza en `scripts/5_Mission/LFPG_BTCHelper.c:2415`. La búsqueda final arroja cero referencias a la función eliminada. Sin nuevas llamadas al engine.

**Puerta de cierre:** `python3 gate_astra.py` devolvió `PUERTA OK`, con ocho ficheros modificados e informe válido. No compila el mod.

**Comprobaciones ejecutadas:** revisión del diff frente a baseline y aserciones estáticas para C1–C5: PASS. Se comprobaron orden de normalización, guards, retorno parcial, herencia de ambos ATM, límite del bucle, rollback, ausencia de llamadas World→Mission y ausencia de nuevos `ref`. Un modelo aritmético Python pasó 576 combinaciones y ocho casos de borde: stock cero, 40 unidades en pilas de 25, fallos null, rechazo del descuento, no apilables y exceso sobre el tope. Ese modelo no ejecuta Enforce ni prueba el engine.

## LO_NO_VERIFICADO

- No compilé ni cargué el mundo: aquí no hay DayZ ni compilador Enforce. La puerta no sustituye AddonBuilder y un arranque real.
- No ejecuté el round-trip real `FPrintln`→`FGets` con CRLF/LF. `Trim` se eligió por el contrato vanilla de whitespace; no se midió su implementación nativa.
- No probé en servidor el desmontaje con energía/combustible ni una reconciliación de venta parcial y reconexión. La marca de C3 no es durable: el administrador debe resolver el parcial antes de reiniciar; no se garantiza una recuperación automática entre arranques.
- No probé la muerte por daño de ambos ATM, accesibilidad del botín, jitter, física, sincronización ni cantidades de la configuración desplegada. Los overrides de otros mods podrían cambiar el comportamiento vanilla leído.
- C5 supone que las pilas creadas siguen válidas durante el hook síncrono. `ObjectDelete` no confirma éxito; el rollback y los fallos de escritura del diario necesitan prueba real.
- C5 no hace una transacción durable conjunta entre objetos, stock del mundo y diario. La guarda solo cubre la instancia viva. No se garantiza exactamente una entrega si hay un crash entre spawn, descuento y persistencia.
- Si no cabe todo en 64 pilas, fallan los spawns o el diario rechaza el descuento, puede quedar stock en un ATM arruinado. El error permite intervención, pero ese residual puede perderse con la limpieza del objeto. No se añadió recuperación durable ni reintento automático fuera del encargo.
- La ausencia de referencias a `DestroyPlayerCash` se comprobó en todo `scripts/`, no en mods externos ni mediante enlazado real.
