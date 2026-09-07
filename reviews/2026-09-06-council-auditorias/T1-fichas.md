#### E04

**Sell a cuenta pierde BTC si falla el guardado del crédito después de destruir los objetos**

**Referencias al commit:** `scripts/4_World/LFPG_BTCHelper.c`: [L1425–1473](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L1425-L1473) · `scripts/4_World/LFPG_BalanceProvider_Native.c`: [L1688–1744](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L1688-L1744).


**Severidad:** Alta. **Confianza:** Confirmado por flujo de fallo.

**Ubicación:** `scripts/4_World/LFPG_BTCHelper.c:1425–1473`; `scripts/4_World/LFPG_BalanceProvider_Native.c:1688–1744`.

**Evidencia y escenario:** Sell destruye los BTC en 1426; después llama a AddBalance en 1466. Native.AddBalance vuelve al saldo anterior y devuelve 0 cuando SaveBalanceMutation falla (1725-1733). El handler responde INVALID en 1467-1473, sin devolver los BTC ni guardar una obligación pendiente por el crédito que faltó. La comprobación IsClaimStoreWritable anterior no garantiza que la siguiente escritura vaya a funcionar. La rama destroyed!=btcAmount también elimina salidas sin restituir los BTC ya destruidos.

**Impacto:** Pérdida de valor del jugador ante disco lleno, error de escritura o, para cardinalidad parcial, modificaciones del inventario durante callbacks. El fallo de escritura basta para el caso principal; no requiere una carrera multihilo.

**Cambio propuesto sin perder garantías:** Introducir una transacción de venta con intención/obligación persistente e identificadores que permitan reconciliar inventario y saldo. Como mitigación síncrona, validar y reservar entradas, hacer crédito preparado y persistencia con rollback explícito, y no considerar resuelto un error hasta restituir exactamente o dejar un claim durable. Cambiar simplemente el orden crédito/destrucción desplaza el riesgo a duplicación tras crash.

**Verificación de regresión:** Inyección de fallo en cada fase del guardado del crédito, incluido después de staging de cash por overflow de cuenta; verificar BTC+EUR conservados o una obligación de recuperación exacta. Caso de consumo parcial y segundo reinicio durante recuperación.

#### E16

**La compensación de claims REFUNDED elimina el tombstone antes de demostrar que el stock compensado quedó persistido en hive**

**Referencias al commit:** `scripts/4_World/LFPG_BalanceProvider_Native.c`: [L1212–1242](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L1212-L1242) · `scripts/4_World/LFPG_BTCAtm.c`: [L165–177](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCAtm.c#L165-L177), [L269–275](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCAtm.c#L269-L275).


**Severidad:** Alta. **Confianza:** POTENCIAL — requiere prueba de crash y orden real de persistencia DayZ.

**Ubicación:** `scripts/4_World/LFPG_BalanceProvider_Native.c:1212–1242`; `scripts/4_World/LFPG_BTCAtm.c:165–177`; `scripts/4_World/LFPG_BTCAtm.c:269–275`.

**Evidencia y escenario:** ReconcileRefundedChain aplica LFPG_ApplyClaimedStockTarget(restoredStock) y a continuación PersistRemoveDeviceClaimPrefix borra los tombstones del JSON. ApplyClaimedStockTarget cambia m_BtcStock y SetSynchDirty; el stock se serializa en OnStoreSaveExtra, mecanismo diferente. A diferencia del reapply de purchases, aquí no se conserva la prueba hasta un siguiente arranque.

**Impacto:** Si un ATM reaparece con stock ya reembolsado y el proceso cae después del clear JSON pero antes de persistir el stock compensado, la siguiente carga podría recuperar stock antiguo sin tombstone que lo revierta. Eso permitiría saldo devuelto y BTC todavía disponibles. No se ejecutó un crash test y no se afirma que SetSynchDirty fuerce un guardado durable.

**Cambio propuesto sin perder garantías:** Conservar un tombstone de compensación pendiente con target y generación hasta prueba durable de hive/arranque posterior, o mover ambos estados a una transacción/journal común. Hacer idempotente la reaplicación de la compensación y solo entonces compactar.

**Verificación de regresión:** ATM ausente que fue refunded y reaparece; crash justo tras aplicar stock, durante clear JSON y después; segunda recuperación y retiro. Comprobar que reembolso+stock total nunca se duplica.

#### SEC09

**Un guardado que devuelve fallo puede confirmarse en el siguiente arranque**

**Referencias al commit:** `scripts/3_Game/LFPG_FileUtil.c`: [L251–263](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_FileUtil.c#L251-L263), [L546–553](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_FileUtil.c#L546-L553) · `scripts/4_World/LFPG_BalanceProvider_Native.c`: [L1723–1733](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L1723-L1733) · `scripts/4_World/LFPG_BTCHelper.c`: [L2254–2260](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L2254-L2260).


**Alta · CONFIRMADO el contrato contradictorio; escenario de E/S pendiente de prueba DayZ · Confianza alta.**

**Evidencia:** `scripts/3_Game/LFPG_FileUtil.c:251–263,546–553`; `scripts/4_World/LFPG_BalanceProvider_Native.c:1723–1733`; `scripts/4_World/LFPG_BTCHelper.c:2254–2260`. El mismo protocolo de promoción fallida existe en FileUtil 91–105 y 174–186.

AtomicSaveBalances devuelve false si falla CopyFile(tmp,target), restaura el anterior pero conserva el `.tmp` válido. Native.AddBalance interpreta false como no durable y revierte RAM. DepositCash sale sin retirar el efectivo. Si se reinicia antes de otro guardado exitoso, EnsureBalances promociona ese `.tmp` como último estado aunque el target anterior se hubiese restaurado correctamente: acredita el depósito que se comunicó como fallido y cuyo efectivo nunca se retiró. En un débito aparece el riesgo inverso. Se requiere fallo de E/S y reinicio en la ventana; no se atribuye al jugador capacidad para provocar el fallo de disco. No exige crash ni target ausente: si dirtyBefore era false, el rollback restaura false y FlushBalanceOnShutdown (1638–1642) no escribe, de modo que también un reinicio limpio puede conservar el `.tmp`.

**Cambio:** definir resultado de persistencia `committed / aborted / uncertain`, con journal o marca durable de intención/commit que la recuperación pueda distinguir. Si no puede garantizarse abort, bloquear nuevas transacciones y reconciliar en lugar de devolver un fallo que induce rollback unilateral. Preservar un `.tmp` abortado fuera de la ruta de promoción automática solo si se garantiza que su estado no quedó aplicado; borrar sin más tampoco resuelve fallos inciertos.

**Regresión:** inyectar fallo en cada SaveFile/CopyFile/DeleteFile y reiniciar en cada frontera; saldo+efectivo/stock siempre conservados, replay coherente y nunca promoción de una transacción abortada. Modelo local de ramas: RAM volvió a 100 tras fallo; el siguiente boot promovió `.tmp=200`.

#### E02

**La compra en efectivo crea entidades antes de comprobar que el jugador puede pagarlas**

**Referencias al commit:** `scripts/4_World/LFPG_BTCHelper.c`: [L1065–1110](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L1065-L1110), [L449–564](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L449-L564) · `scripts/3_Game/LFPG_BTCConfig.c`: [L206–220](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_BTCConfig.c#L206-L220).


**Severidad:** Alta. **Confianza:** Confirmado; gravedad práctica condicionada a la configuración.

**Ubicación:** `scripts/4_World/LFPG_BTCHelper.c:1065–1110`; `scripts/4_World/LFPG_BTCHelper.c:449–564`; `scripts/3_Game/LFPG_BTCConfig.c:206–220`.

**Evidencia y escenario:** HandleBTCBuy cash reserva el nonce y llama a StageItemsForPlayer en 1076; PreparePlayerCash/SelectPreparedCashCover solo llegan después, en 1091/1100. Un jugador sin billetes puede producir altas y bajas de entidades en cada solicitud nueva admitida. El servidor limita la frecuencia y btcAmount, pero maxBtcPerMachine se permite hasta 10000 y el propio código admite items no apilables.

**Impacto:** Carga evitable de creación/inventario/física y borrado, incluso en operaciones destinadas a fallar. Existe una vía de abuso de rendimiento con sesiones válidas y configuraciones de artículos caros de crear; no se afirma bypass del rate limit ni caída reproducida.

**Cambio propuesto sin perder garantías:** Preparar primero entradas y capacidad de pago. Para conservar las compras parcialmente entregadas que hoy se permiten, calcular el máximo asequible y una planificación sin efectos físicos; si se exige el total solicitado, documentar ese cambio de política. Solo después crear salidas y ajustar el débito al número realmente creado. Mantener reserva antes del primer efecto.

**Verificación de regresión:** Jugador sin dinero y solicitud máxima: cero entidades creadas. Fondos parciales, creación parcial, falta de cambio, inventario lleno y borrado de salidas al abortar. Confirmar que no cambia accidentalmente la política de entrega parcial.

#### E03

**Los límites monetarios no limitan el número de entidades generadas por una transacción**

**Referencias al commit:** `scripts/4_World/LFPG_BTCHelper.c`: [L478–546](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L478-L546), [L580–628](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L580-L628), [L1271–1290](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L1271-L1290), [L1399–1408](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L1399-L1408) · `scripts/3_Game/LFPG_BTCConfig.c`: [L206–230](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_BTCConfig.c#L206-L230).


**Severidad:** Alta. **Confianza:** Confirmado; impacto según denominaciones y capacidad de pila.

**Ubicación:** `scripts/4_World/LFPG_BTCHelper.c:478–546`; `scripts/4_World/LFPG_BTCHelper.c:580–628`; `scripts/4_World/LFPG_BTCHelper.c:1271–1290`; `scripts/4_World/LFPG_BTCHelper.c:1399–1408`; `scripts/3_Game/LFPG_BTCConfig.c:206–230`.

**Evidencia y escenario:** StageItemsForPlayer tiene un while hasta agotar amount; si maxStack=1 solo emite un warning, sin presupuesto. GreedyChange convierte importe en billetes por denominación y llama a ese while. El límite maxEurPerOperation se aplica a WithdrawCash/DepositCash, no al efectivo producido por Sell ni al spill de Sell a cuenta; Sell permite un producto precio*cantidad hasta 2.000.000.000. Con una divisa de valor 1 y pila 1, el número potencial de entidades es ese importe; el resultado depende de lo que el motor llegue a crear.

**Impacto:** Una operación legítima puede bloquear el frame durante una ráfaga enorme de spawns e intentar llenar el suelo. Un límite de BTC o EUR no es un límite de trabajo.

**Cambio propuesto sin perder garantías:** Estimar primero cantidades y número de pilas de todas las salidas; imponer un máximo por transacción de entidades, búsquedas de hueco y drops. Rechazar antes de mutar si se excede. Usar una capacidad de pila validada/cacheada por classname tras un probe controlado, sin asumir que todas las clases viven en CfgVehicles. No repartir una transacción por frames sin introducir un protocolo transaccional duradero.

**Verificación de regresión:** Moneda valor 1 y no apilable; denominaciones muy pequeñas; cuenta al máximo que derrama a cash; BTC no apilable; límite justo y límite+1. Medir tiempo de peor caso y asegurar cero efectos al rechazar.

#### E08

**La configuración de monedas admite classnames duplicados, solapados con BTC o completamente inválidos**

**Referencias al commit:** `scripts/3_Game/LFPG_BTCConfig.c`: [L274–330](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/3_Game/LFPG_BTCConfig.c#L274-L330) · `scripts/4_World/LFPG_BTCHelper.c`: [L58–68](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L58-L68), [L160–199](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L160-L199), [L682–739](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L682-L739).


**Severidad:** Alta. **Confianza:** Confirmado para validación/contabilización; explotación concreta condicionada a configuración y motor.

**Ubicación:** `scripts/3_Game/LFPG_BTCConfig.c:274–330`; `scripts/4_World/LFPG_BTCHelper.c:58–68`; `scripts/4_World/LFPG_BTCHelper.c:160–199`; `scripts/4_World/LFPG_BTCHelper.c:682–739`.

**Evidencia y escenario:** ValidateAndClamp elimina nulls después del fallback, conserva classnames vacíos y no detecta duplicados ni colisión con btcItemClassname. Una lista [null] termina sin monedas válidas. Un classname duplicado hace CountPlayerCash sumar la misma pila dos veces y PreparePlayerCash insertar dos referencias a la misma entidad. Prevalidar cada referencia por separado no detecta que la suma seleccionada supera la pila. Si moneda y BTC coinciden, cash Buy puede incluir sus propios BTC recién creados en las entradas, porque prepara cash después de staging.

**Impacto:** Balances visuales inflados y planes de consumo incorrectos; posible crédito sin respaldo o destrucción inconsistente según vida de las referencias de entidades. No es edición de config por un jugador remoto: el prerequisito es una configuración administrativa inválida que hoy el loader acepta.

**Cambio propuesto sin perder garantías:** Normalizar y validar todo el catálogo antes de activarlo: classname no vacío y existente, una denominación por classname, BTC separado de moneda, valores y tamaño de catálogo acotados. Comprobar de nuevo no vacío después de filtrar. Además, el plan debe agrupar por identidad de entidad y validar consumo total por entidad para defensa adicional.

**Verificación de regresión:** Duplicados iguales/distintos, [null], classnames vacíos/inexistentes, BTC=moneda, misma entidad repetida en un plan y denominaciones máximas. Rechazo explícito antes de operaciones, sin sustituir silenciosamente valores económicos incompatibles.

#### E05

**DepositCash persiste un crédito antes de comprobar que ese importe es representable por los billetes disponibles**

**Referencias al commit:** `scripts/4_World/LFPG_BTCHelper.c`: [L2240–2282](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BTCHelper.c#L2240-L2282) · `scripts/4_World/LFPG_BalanceProvider_Native.c`: [L1688–1801](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L1688-L1801).


**Severidad:** Media. **Confianza:** Confirmado; posible crédito indebido si falla la compensación.

**Ubicación:** `scripts/4_World/LFPG_BTCHelper.c:2240–2282`; `scripts/4_World/LFPG_BalanceProvider_Native.c:1688–1801`.

**Evidencia y escenario:** El handler comprueba solo valor agregado, ejecuta AddBalance en 2254 y selecciona billetes exactos en 2263. Por ejemplo, llevar solo un billete de 100 e ingresar 60 supera el chequeo de fondos pero obliga a un AddBalance+RemoveBalance cuando no hay selección exacta. Native guarda un snapshot global en ambas operaciones. Si el segundo guardado falla, RemoveBalance revierte su débito y puede quedar crédito sin consumir billetes; actualmente solo se registra un error.

**Impacto:** Dos guardados evitables para un rechazo previsible, más ventana de inconsistencia. También ocurre cuando el límite de saldo hace que el proveedor abone solo un importe parcial no representable.

**Cambio propuesto sin perder garantías:** Para Native, calcular primero crédito admisible por room y comprobar selección exacta antes del primer guardado. Para proveedores externos añadir una capacidad de preflight/commit con contrato explícito; si no existe, usar un intento preparado/reconciliable. Agrupar mutaciones síncronas solo con rollback de todas las entidades y garantías de persistencia, sin debounce arbitrario.

**Verificación de regresión:** Billete único de 100/ingreso60, saldo a 30 del máximo, denominaciones sin unidad, fallo de compensación y proveedor que devuelve crédito parcial. Verificar cero saves para rechazos previsibles y conservación del dinero.

#### E15

**Ocho compras a cuenta bloquean más compras en el mismo ATM hasta una futura reconciliación de arranque**

**Referencias al commit:** `scripts/4_World/LFPG_BalanceProvider_Native.c`: [L64–67](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L64-L67), [L203–214](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L203-L214), [L418–424](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L418-L424), [L519–598](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L519-L598), [L1168–1179](https://github.com/willy92wins/LFPowerGrid/blob/d61705eb862a9781393a94335fab7315a4fd375a/scripts/4_World/LFPG_BalanceProvider_Native.c#L1168-L1179).


**Severidad:** Media. **Confianza:** Confirmado como límite de diseño; conveniencia funcional a validar.

**Ubicación:** `scripts/4_World/LFPG_BalanceProvider_Native.c:64–67`; `scripts/4_World/LFPG_BalanceProvider_Native.c:203–214`; `scripts/4_World/LFPG_BalanceProvider_Native.c:418–424`; `scripts/4_World/LFPG_BalanceProvider_Native.c:519–598`; `scripts/4_World/LFPG_BalanceProvider_Native.c:1168–1179`.

**Evidencia y escenario:** Cada compra a cuenta agrega un claim PENDING y CountPendingPurchaseClaims limita a ocho por device. En runtime PrepareStockMutation fusiona/añade segmentos físicos pero no libera purchases. La prueba de entrega y limpieza ocurre en ReconcileLoadedAtm/boot. Alternar pequeñas compras y retiros puede dejar el ATM con stock disponible pero sin más compras a cuenta tras ocho compras entre arranques.

**Impacto:** El límite protege la complejidad/reconciliación, pero funciona como cuota acumulada por ATM y periodo entre reinicios, compartida entre usuarios; el cliente recibe INVALID genérico y no explica la saturación. Cambiar el número solo aplaza el problema.

**Cambio propuesto sin perder garantías:** Mantener el fail-closed mientras no exista una prueba durable más eficiente. Mostrar un error específico y capacidad pendiente; diseñar checkpoints de stock con proof/generación que permitan compactar claims confirmados en runtime. No borrar claims solo porque stock RAM coincide: se perdería la garantía frente a crash.

**Verificación de regresión:** Nueve compras de1 BTC, intercaladas con retiros para que quede hueco; varios jugadores en un ATM; reinicio limpio y abrupto. Verificar que la futura compactación no pierde ni duplica compras.