# ENCARGO — tercera pasada: un solo arreglo

## El defecto
Tu reconciliación de E04 (`LFPG_BTCHelper.c:1455-1468`, `ReconcilePendingAccountSell`) destruye
los BTC y **después** intenta borrar el marcador. Si `ClearSellDestroyIntent` falla, solo registras
un error pidiendo intervención del admin — pero el marcador sobrevive **y el saldo no se ha
movido**. En la siguiente reconexión del jugador, `current` vuelve a valer
`balanceBefore + creditAmount`, la comparación vuelve a cuadrar, y **destruyes otros `btcAmount`
BTC**. Y otra vez en la siguiente reconexión, hasta que un admin borre el fichero a mano.

El mismo camino existe en la venta normal: si el `ClearSellDestroyIntent` de después de
`DestroyPlayerItems` falla, el siguiente connect vuelve a destruir.

Esto es **peor que el E04 original**. Aquel perdía dinero una vez ante un fallo concreto; esto lo
pierde de forma repetida. Es una vía de pérdida nueva, introducida por el arreglo.

## El arreglo
No dependas del borrado, que es justo la operación que acaba de fallar. **Depende de la
escritura**, que es la que sí funcionó al crear el marcador.

Después de destruir: si `ClearSellDestroyIntent` falla, **reescribe el marcador** poniendo
`balanceBefore` al saldo **actual** (el de después del crédito). Así la siguiente pasada entra por
la rama `current == balanceBefore`, que ya tienes escrita en `:1441-1448` y que limpia el marcador
**dejando los objetos intactos**. Si esa reescritura también falla, entonces sí: log de admin, que
es lo único que queda.

Aplica lo mismo en el camino de venta normal, donde el `Clear` de después de la destrucción tiene
el mismo modo de fallo.

## Fronteras
- **Solo esto.** No reabras E16, SEC09, E02, E03, E05 ni E08. No toques nada más.
- Repo `P:\LFPowerGrid`, rama `fix/t1-integridad-monetaria` ya activa. **PROHIBIDO tocar git**,
  compilar, empaquetar o lanzar DayZ.
- Convenciones vigentes: cero ternarios, `x = x + 1`, nada de `+=`/`-=`/`foreach`,
  `LFPG_Util.Error/Warn/Info/Debug`, regla del escape simple.
- **G6 fail-closed:** ante cualquier estado que no puedas interpretar sin ambigüedad, **no
  destruyas**. Que un marcador ilegible o un saldo raro no cueste BTC a nadie.

## Entrega
Añade a `reviews/2026-09-06-council-auditorias/IMPL-T1-INFORME.md`, al final:

```
## TERCERA PASADA
### E04 — marcador re-armado en vez de borrado
- **Qué cambié:** path:line
- **Qué sigue abierto:** el borde exacto que quede
```

Si crees que el arreglo propuesto no funciona o que hay una vía mejor, **dilo y argumenta con
`path:line`** en vez de implementarlo a ciegas.
