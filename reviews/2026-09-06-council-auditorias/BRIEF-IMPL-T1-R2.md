# ENCARGO — segunda pasada sobre T1 (integridad monetaria)

## 1. Qué ha pasado
Escribiste el parche de T1. Un revisor de otra familia lo ha leído entero, línea a línea, contra
el árbol y contra `P:\scripts` vanilla. Su informe está en
`reviews/2026-09-06-council-auditorias/REVIEW-T1.md` — **léelo antes de tocar nada.**

Su veredicto fue **«no desplegar tal cual»**, con tres GRAVES. Uno de ellos (E08) no es tuyo: es
un gate de despliegue que resuelve el dueño mirando el JSON de producción. **Los otros dos son
tuyos y son este encargo.**

Buena noticia primero, porque cambia dónde merece la pena tu esfuerzo: el revisor contrastó tus
firmas una a una y no encontró ninguna inventada; confirmó que no tocaste E15; verificó que las
convenciones están limpias en las 627 líneas; y encontró que tu residuo de SEC09 es **más
estrecho** de lo que tú mismo declaraste — hacen falta unos cinco fallos de E/S simultáneos, no
tres. **SEC09, E02, E03 y E05 no se reabren.** No los toques.

## 2. Entorno — idéntico al encargo anterior
- Repo `P:\LFPowerGrid`, **Enforce Script**, rama `fix/t1-integridad-monetaria` ya activa con tus
  cambios sin commitear. **PROHIBIDO tocar git**, compilar, empaquetar o lanzar DayZ.
- Convenciones que ya respetaste y siguen vigentes: cero ternarios, `x = x + 1` en vez de `++`,
  nada de `+=`/`-=`/`foreach`, `LFPG_Util.Error/Warn/Info/Debug`, regla del escape simple.
- Sigue vigente **G6, fail-closed**: ninguna corrección puede aceptar dato del cliente como bueno
  ni convertir un error en éxito silencioso.

## 3. GRAVE 1 — E16 está mal etiquetada: la cerraste solo para el ATM presente

**Lo que dice el revisor** (`REVIEW-T1.md`, primer hallazgo):

> El fix conserva los tombstones cuando el ATM **presente** recibe la compensación
> (`NativeImpl.c:1240-1241`) y solo compacta cuando el stock hive ya coincide (`:1246-1250`). Pero
> `AdvanceOrPruneRefundedClaimAt` (`:1380`) sigue podando el tombstone cuando
> `bootsSinceRefund + 1 >= 3`, y `SweepOrphanClaims` (`:1532-1537`) ejecuta esa poda precisamente
> para ATMs **ausentes**.

El camino concreto que deja abierto, y que es **el escenario literal de la ficha**:

1. Una compra a cuenta aplica stock 10→15 y persiste en hive.
2. El ATM queda ausente (empaquetado, o fuera de burbuja).
3. Dos sweeps lo ven ausente → se devuelve el débito al jugador y se pone tombstone (boot N).
4. Boots N+1 y N+2 incrementan `bootsSinceRefund`; en N+3 se **poda** el tombstone.
5. El ATM reaparece en N+4 con `m_BtcStock=15` en hive. `ReconcileLoadedAtm` encuentra la cadena
   vacía (`:1271-1274`) y lo da por reconciliado **sin compensar**.
6. Resultado: el jugador tiene el reembolso **y** el ATM tiene 5 BTC retirables. **Duplicación.**

**Lo que tienes que hacer:** que la poda de un tombstone REFUNDED no pueda ocurrir mientras siga
siendo la única prueba de que hay una compensación pendiente. Tú ya escribiste el patrón correcto
para el caso presente; extiéndelo al ausente.

Si concluyes que no se puede sin tocar E15 o el protocolo de huérfanos, **dilo y no lo fuerces**:
un `NO_CERRADA` con el motivo vale más que un arreglo que rompa la cuota de PENDING. Pero
entonces cambia el encabezado de tu informe: no puede seguir diciendo `CERRADA`.

## 4. GRAVE 2 — E04: acotar la ventana de duplicación, o firmarla explícitamente

**Lo que dice el revisor:**

> Crash del proceso después de que `AtomicSaveBalances` complete (crédito durable) y antes de la
> destrucción: al reiniciar el jugador tiene el crédito **y** los BTC intactos. […] la ventana de
> duplicación queda **abierta y es nueva** respecto al comportamiento anterior (que perdía, no
> duplicaba). En una economía viva, dup > pérdida como riesgo sistémico.

Tu reorden es legítimo y el escenario que la ficha reportaba queda cerrado — eso no se discute.
El problema es que cambiaste la **dirección** del fallo y eso, con jugadores dentro, es una
decisión que hay que tomar con los ojos abiertos.

**Lo que tienes que hacer, en este orden de preferencia:**

1. **Acotar la ventana con un marcador de en-vuelo.** Ya escribiste exactamente ese patrón para
   SEC09: el marcador `target + ".saving"` que distingue un crash a mitad de un abort reportado.
   Aplica la misma idea a la venta: dejar constancia durable de «crédito aplicado, destrucción
   pendiente» antes de `AddBalance`, y retirarla después de `DestroyPlayerItems`. Un arranque que
   encuentre ese rastro puede reconciliar en vez de regalar.
2. **Si eso exige un claim o journal** que choque con el tope de 8 PENDING de E15: **no lo hagas.**
   En su lugar, documenta la ventana con precisión en tu informe —cuántas instrucciones dura, qué
   estado queda, y qué haría falta para cerrarla— para que el dueño la firme como decisión de
   producto.

No inventes una API nueva del proveedor de saldo para esto.

## 5. Menores, solo si son baratos y en los ficheros que ya tocaste
Ninguno bloquea. Hazlos si salen en una línea; si alguno se complica, déjalo y dilo.

- **Duplicados sensibles a mayúsculas** (`LFPG_BTCConfig.c:325-334`): el motor resuelve classnames
  sin distinguir mayúsculas, tu `Find` sí. `Paper_Bill_100` y `PAPER_BILL_100` pasan como
  distintos siendo la misma clase. Normaliza antes del `Find`; ya usas ese patrón en `:264`.
- **Helpers muertos sin las guardas nuevas** (`LFPG_BTCHelper.c:2197-2268` `DestroyPlayerCash`,
  `:458-461` `CreateItemsForPlayer`): se quedaron sin call-sites tras tu reescritura, y
  `DestroyPlayerCash` no consulta `IsCurrencyCatalogValid`. O los borras, o les pones la guarda.
  Borrarlos es mejor: cada clase y cada método cuestan arena de script.
- **Basura acumulativa** (`LFPG_FileUtil.c:417-439`): los `.tmp.preserved.<ts>_<rnd>` no los limpia
  nadie y se acumulan en `$profile:` tras cada abort. Un barrido acotado al arrancar basta.

## 6. Cómo entregar
Actualiza `reviews/2026-09-06-council-auditorias/IMPL-T1-INFORME.md` **en su sitio**: corrige el
encabezado de E16 si procede, añade lo nuevo de E04, y añade una sección al final:

```
## SEGUNDA PASADA (respuesta a REVIEW-T1.md)
### <ID> — <qué hiciste ahora>
- **Qué cambié:** path:line
- **Qué sigue abierto:** el borde exacto que queda
```

Y actualiza `LO_NO_VERIFICADO` con lo que esta pasada añada.

**Si discrepas del revisor en algo, dilo y argumenta con `path:line`.** No es una autoridad: es
otra lectura. Si crees que se equivocó en un camino de ejecución, demuéstralo — eso es tan útil
como un arreglo.
